#include "pugixml.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>

namespace matcher
{
    namespace keys
    {
        using Key = std::size_t;

        template <typename KeyT>
        const std::hash<KeyT>& GetHasher()
        {
            static const std::hash<KeyT> hasher{};
            return hasher;
        }

        namespace detail
        {
            template <typename LhsKeyT, typename RhsKeyT>
            void combineKeysUniquelyImpl(LhsKeyT& lhsKey, const RhsKeyT& rhsKey)
            {
                lhsKey ^= GetHasher<RhsKeyT>()(rhsKey) +
                          0x9e3779b9 +
                          (lhsKey << 6) +
                          (lhsKey >> 2);
            }
        }

        template <typename... KeysT>
        Key combineKeysUniquely(const KeysT&... keys)
        {
            Key result{};
            (detail::combineKeysUniquelyImpl(result, keys), ...);

            return result;
        }

        template <typename... KeysT>
        Key combineKeysLoosely(const KeysT&... keys)
        {
            return (keys ^ ...);
        }
    }

    namespace detail
    {
        keys::Key computeAttributesKey(const pugi::xml_node& node)
        {
            const auto& stringHasher = keys::GetHasher<std::string>();

            keys::Key result{};
            for (auto attributeItr = node.first_attribute(); attributeItr; attributeItr = attributeItr.next_attribute())
            {
                result = keys::combineKeysLoosely(result,
                                                  keys::combineKeysUniquely(stringHasher(attributeItr.name()),
                                                                            stringHasher(attributeItr.value())));
            }

            return result;
        }

        keys::Key computeNodeKey(const pugi::xml_node& node)
        {
            const auto childrenKey = std::accumulate(node.begin(),
                                                     node.end(),
                                                     keys::Key{},
                                                     [](const auto& partialKey, const auto& child)
                                                     {
                                                        return keys::combineKeysLoosely(partialKey, computeNodeKey(child));
                                                     });

            const auto& stringHasher = keys::GetHasher<std::string>();
            return keys::combineKeysUniquely(stringHasher(node.name()),
                                             stringHasher(node.child_value()),
                                             computeAttributesKey(node),
                                             childrenKey);
        }
    }

    bool matchXMLDocumentsLoosely(const pugi::xml_document& lhsDocument, const pugi::xml_document& rhsDocument)
    {
        const auto lhsDocumentKey = detail::computeNodeKey(lhsDocument.document_element());
        const auto rhsDocumentKey = detail::computeNodeKey(rhsDocument.document_element());

        return lhsDocumentKey == rhsDocumentKey;
    }
}

namespace test
{
    const std::filesystem::path& GetTestDataDirectory()
    {
        static const std::filesystem::path directory{"test_data"};
        return directory;
    }

    void loadXmlFile(const std::string& xmlFile, pugi::xml_document& xmlDocument)
    {
        const auto fullXmlFilePath = GetTestDataDirectory() / xmlFile;
        const auto loadResult = xmlDocument.load_file(fullXmlFilePath.c_str());
        if (!loadResult)
        {
            std::cout << "Failed to load XML file `"
                      << fullXmlFilePath
                      << "`: `"
                      << loadResult.description()
                      << "`"
                      << std::endl;

            std::abort();
        }
    }

    void executeTest(const std::string& lhsXmlFile, const std::string& rhsXmlFile, bool expectedEquivalency)
    {
        pugi::xml_document lhsDocument{};
        loadXmlFile(lhsXmlFile, lhsDocument);

        pugi::xml_document rhsDocument{};
        loadXmlFile(rhsXmlFile, rhsDocument);

        std::cout << "["
                  << lhsXmlFile
                  << "] "
                  << (expectedEquivalency ? "==" : "!=")
                  << " ["
                  << rhsXmlFile
                  << "] ---> ";

        if (matcher::matchXMLDocumentsLoosely(lhsDocument, rhsDocument) == expectedEquivalency)
        {
            std::cout << "PASSED" << std::endl; 
        }
        else
        {
            std::cout << "FAILED" << std::endl;
        }
    }

    void runTests()
    {
        std::cout << "\n\n";

        executeTest("1.xml", "2.xml", true);
        executeTest("3.xml", "4.xml", false);
        executeTest("5.xml", "6.xml", true);
        executeTest("7.xml", "8.xml", true);
        executeTest("9.xml", "10.xml", true);
        executeTest("11.xml", "12.xml", false);
        executeTest("13.xml", "14.xml", true);
        executeTest("15.xml", "16.xml", true);
        executeTest("17.xml", "18.xml", false);

        std::cout << "\n" << std::endl;
    }
}

int main()
{
    test::runTests();

    return 0;   
}
