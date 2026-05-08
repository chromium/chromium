// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "services/data_decoder/public/cpp/xml_dom.h"
#include "services/data_decoder/xml_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace data_decoder::xml {

namespace {

class XmlParserRsTest : public testing::Test {
 protected:
  // Most test cases should call this method, which ensures that the Rust parser
  // and the legacy libxml2-based parser produce identical results.
  //
  // This rather confusing looking signature consists of two parts:
  // 1. The outer `base::expected` represents whether or not the legacy and Rust
  //    parser interpreted the XML the same way.
  // 2. The inner `base::expected` is the result from the Rust parser; in the
  //    case of invalid XML, this will contain a string that (theoretically)
  //    provides helpful additional context.
  base::expected<base::expected<Document, std::string>, std::string> ParseXml(
      std::string_view xml) {
    base::expected<Document, std::string> rust_result = ParseWithRust(xml);
    base::expected<base::Value, std::string> libxml2_result =
        ParseWithLibxml2(xml);

    if (rust_result.has_value() != libxml2_result.has_value()) {
      if (rust_result.has_value()) {
        return base::unexpected(
            "Rust parser succeeded, but libxml2 parser failed: " +
            libxml2_result.error());
      }
      return base::unexpected(
          "libxml2 parser succeeded, but Rust parser failed: " +
          rust_result.error());
    }

    if (rust_result.has_value()) {
      base::Value rust_result_as_value = rust_result->ToValueForTesting();
      if (rust_result_as_value != libxml2_result) {
        return base::unexpected(
            base::StrCat({"Mismatch for input: ", xml,
                          "\nRust result: ", rust_result_as_value.DebugString(),
                          "\nC++ result:  ", libxml2_result->DebugString()}));
      }
    }

    return rust_result;
  }

 private:
  base::expected<Document, std::string> ParseWithRust(std::string_view xml) {
    return Document::FromUtf8(xml);
  }

  base::expected<base::Value, std::string> ParseWithLibxml2(
      std::string_view xml) {
    XmlParser parser_impl;
    // `XmlParser`'s overrides for `mojom::XmlParser` are private so cheat to
    // make them visible.
    mojom::XmlParser& parser = parser_impl;

    std::optional<base::Value> value;
    std::optional<std::string> error;
    // This call does not actually go over Mojo, so the results will be
    // available synchronously.
    parser.Parse(std::string(xml),
                 mojom::XmlParser::WhitespaceBehavior::kIgnore,
                 base::BindLambdaForTesting(
                     [&](std::optional<base::Value> maybe_value,
                         const std::optional<std::string>& maybe_error) {
                       value = std::move(maybe_value);
                       error = std::move(maybe_error);
                     }));

    // The legacy C++ parser promises to only set exactly one of `cpp_value`
    // or `cpp_error`, so make sure that's the case.
    CHECK_NE(value.has_value(), error.has_value());

    if (value) {
      return *std::move(value);
    }
    return base::unexpected(*std::move(error));
  }
};

TEST_F(XmlParserRsTest, Basic) {
  ASSERT_OK_AND_ASSIGN(
      auto doc, ParseXml("<root><child attr=\"value\">text</child></root>"));
  ASSERT_OK(doc);
  const Node* root = doc->GetRoot();

  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetName(), Name{"root"});

  const auto& children = root->GetChildren();
  ASSERT_EQ(children.size(), 1u);
  const Node* child = children[0].get();
  ASSERT_EQ(child->GetType(), Node::Type::kElement);
  EXPECT_EQ(child->GetName(), Name{"child"});
  const auto* attr = child->GetAttribute(Name{"attr"});
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(*attr, "value");

  const auto& grandchildren = child->GetChildren();
  ASSERT_EQ(grandchildren.size(), 1u);
  const Node* grandchild = grandchildren[0].get();
  ASSERT_EQ(grandchild->GetType(), Node::Type::kText);
  EXPECT_EQ(grandchild->GetTextContent(), "text");
}

TEST_F(XmlParserRsTest, MultipleAttributes) {
  ASSERT_OK_AND_ASSIGN(auto doc,
                       ParseXml("<root a=\"1\" b=\"\" c=\"3\"></root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  EXPECT_THAT(root->GetAttributes(),
              UnorderedElementsAre(Pair(Name{"a"}, "1"), Pair(Name{"b"}, ""),
                                   Pair(Name{"c"}, "3")));
}

TEST_F(XmlParserRsTest, DuplicateAttributes) {
  // The same attribute may not be specified multiple times on the same
  // element.
  ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root attr='1' attr='2'></root>"));
  EXPECT_FALSE(doc.has_value());
}

TEST_F(XmlParserRsTest, MixedSiblingTypes) {
  ASSERT_OK_AND_ASSIGN(auto doc,
                       ParseXml("<root><![CDATA[cdata]]><child/>text</root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetName(), Name{"root"});
  ASSERT_EQ(root->GetChildren().size(), 3u);

  ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kCdata);
  EXPECT_EQ(root->GetChildren()[0]->GetTextContent(), "cdata");

  EXPECT_EQ(root->GetChildren()[1]->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetChildren()[1]->GetName(), Name{"child"});

  ASSERT_EQ(root->GetChildren()[2]->GetType(), Node::Type::kText);
  EXPECT_EQ(root->GetChildren()[2]->GetTextContent(), "text");
}

TEST_F(XmlParserRsTest, OmitUnspecifiedDefaultNamespaceOnRoot) {
  // This is a test for backwards compatibility with the original
  // libxml2-based parser.
  ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root />"));
  ASSERT_OK(doc);

  EXPECT_THAT(doc->GetRoot()->GetNamespaces(), IsEmpty());
}

TEST_F(XmlParserRsTest, BasicNamespaces) {
  ASSERT_OK_AND_ASSIGN(auto doc,
                       ParseXml("<foo:root "
                                "    xmlns='https://example.com/default'"
                                "    xmlns:foo='https://example.com/foo'"
                                "    xmlns:bar='https://example.com/bar'"
                                "    bar:attr='fizz' >"
                                "  <bar:child foo:attr='buzz' />"
                                "</foo:root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  EXPECT_THAT(root->GetName(), Eq(Name{.local_name = "root", .prefix = "foo"}));
  EXPECT_THAT(root->GetAttributes(),
              UnorderedElementsAre(
                  Pair(Name{.local_name = "attr", .prefix = "bar"}, "fizz")));
  EXPECT_THAT(root->GetNamespaces(),
              UnorderedElementsAre(Pair("", "https://example.com/default"),
                                   Pair("foo", "https://example.com/foo"),
                                   Pair("bar", "https://example.com/bar")));

  ASSERT_EQ(root->GetChildren().size(), 1u);
  EXPECT_THAT(root->GetChildren()[0]->GetName(),
              Eq(Name{.local_name = "child", .prefix = "bar"}));
  EXPECT_THAT(root->GetChildren()[0]->GetNamespaces(), IsEmpty());
  EXPECT_THAT(root->GetChildren()[0]->GetAttributes(),
              UnorderedElementsAre(
                  Pair(Name{.local_name = "attr", .prefix = "foo"}, "buzz")));
}

TEST_F(XmlParserRsTest, OverrideNamespaceInChild) {
  // TODO(dcheng): This test reveals a behavior difference from the legacy
  // libxml2-based parser, so for now,  do not use the ParseXml() helper.
  auto doc = Document::FromUtf8(
      "<root xmlns:a='https://example.com/1'>"
      "  <child xmlns:a='https://example.com/2'>"
      "    <nested-child xmlns:a='https://example.com/2' />"
      "  </child>"
      "</root>");
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  EXPECT_THAT(root->GetNamespaces(),
              UnorderedElementsAre(Pair("a", "https://example.com/1")));

  const auto& children = root->GetChildren();
  ASSERT_EQ(children.size(), 1u);
  EXPECT_THAT(children[0]->GetNamespaces(),
              UnorderedElementsAre(Pair("a", "https://example.com/2")));

  // Even though `nested-child` explicitly declares the namespace, the
  // declaration is identical to the one inherited from its parent, so it will
  // be omitted.
  const auto& grandchildren = children[0]->GetChildren();
  ASSERT_EQ(grandchildren.size(), 1u);
  EXPECT_THAT(grandchildren[0]->GetNamespaces(), IsEmpty());
}

TEST_F(XmlParserRsTest, UndeclareDefaultNamespaceTest) {
  ASSERT_OK_AND_ASSIGN(auto doc,
                       ParseXml("<root xmlns='https://example.com'><child "
                                "xmlns=''></child></root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  EXPECT_THAT(root->GetNamespaces(),
              UnorderedElementsAre(Pair("", "https://example.com")));

  // While an unspecified default namespace on the root is omitted (see
  // `OmitUnspecifiedDefaultNamespaceOnRoot` above), it should be included on
  // a child element if it the net effect is to clear the default namespace
  // binding.
  ASSERT_EQ(root->GetChildren().size(), 1u);
  EXPECT_THAT(root->GetChildren()[0]->GetNamespaces(),
              UnorderedElementsAre(Pair("", "")));
}

TEST_F(XmlParserRsTest, CdataNode) {
  ASSERT_OK_AND_ASSIGN(
      auto doc,
      ParseXml("<root><![CDATA[some unescaped & chars < >]]></root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  ASSERT_EQ(root->GetChildren().size(), 1u);

  EXPECT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kCdata);
  EXPECT_EQ(root->GetChildren()[0]->GetTextContent(),
            "some unescaped & chars < >");
}

TEST_F(XmlParserRsTest, NestedSiblings) {
  ASSERT_OK_AND_ASSIGN(
      auto doc, ParseXml("<root><a><b></b></a><c><d><e></e></d></c></root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetName(), Name{"root"});
  ASSERT_EQ(root->GetChildren().size(), 2u);
}

TEST_F(XmlParserRsTest, CharacterEntities) {
  ASSERT_OK_AND_ASSIGN(
      auto doc, ParseXml("<root>&lt;tag&gt; &amp; &quot;quoted&quot;</root>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetName(), Name{"root"});
  ASSERT_EQ(root->GetChildren().size(), 1u);

  EXPECT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kText);
  EXPECT_EQ(root->GetChildren()[0]->GetTextContent(), "<tag> & \"quoted\"");
}

// Max depth is 200, so test nesting both at the limit and one over. This
// limit is specific to the Rust-based parser, so this test case does not use
// the `ParseXml()` helper.
TEST_F(XmlParserRsTest, MaxDepth) {
  constexpr int kMaxDepth = 200;
  {
    std::string input;
    for (int i = 0; i < kMaxDepth; ++i) {
      input += "<tag>";
    }
    for (int i = 0; i < kMaxDepth; ++i) {
      input += "</tag>";
    }

    auto doc = Document::FromUtf8(input);
    EXPECT_TRUE(doc.has_value());
  }

  {
    std::string input;
    for (int i = 0; i < kMaxDepth + 1; ++i) {
      input += "<tag>";
    }
    for (int i = 0; i < kMaxDepth + 1; ++i) {
      input += "</tag>";
    }

    auto doc = Document::FromUtf8(input);
    EXPECT_FALSE(doc.has_value());
  }
}

TEST_F(XmlParserRsTest, NoRootElement) {
  // XML documents must have a root element, so empty or all whitespace input
  // should fail to parse.
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml(""));
    EXPECT_FALSE(doc.has_value());
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml(" "));
    EXPECT_FALSE(doc.has_value());
  }

  // The root element... must be an element.
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<!DOCTYPE html>"));
    EXPECT_FALSE(doc.has_value());
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<?pi content ?>"));
    EXPECT_FALSE(doc.has_value());
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<!-- comment -->"));
    EXPECT_FALSE(doc.has_value());
  }
}

TEST_F(XmlParserRsTest, WhitespaceAroundRootElement) {
  ASSERT_OK_AND_ASSIGN(auto doc, ParseXml(" <root></root> "));
  EXPECT_TRUE(doc.has_value());
  const Node* root = doc->GetRoot();

  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetName(), Name{"root"});
}

TEST_F(XmlParserRsTest, MultipleRootElements) {
  // A document can only have one root element.
  ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root></root><root2></root2>"));
  EXPECT_FALSE(doc.has_value());
}

TEST_F(XmlParserRsTest, TextAroundRootElement) {
  // Text nodes at the top-level are invalid in well-formed XML documents.
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("text<root></root>"));
    EXPECT_FALSE(doc.has_value());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root></root>text"));
    EXPECT_FALSE(doc.has_value());
  }
}

TEST_F(XmlParserRsTest, CdataAroundRoot) {
  // Cdata nodes at the top-level are invalid in well-formed XML documents.
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<![CDATA[cdata]]><root></root>"));
    EXPECT_FALSE(doc.has_value());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root></root><![CDATA[cdata]]>"));
    EXPECT_FALSE(doc.has_value());
  }
}

TEST_F(XmlParserRsTest, MismatchedTags) {
  {
    ASSERT_OK_AND_ASSIGN(auto doc, ParseXml("<root><child></root>"));
    EXPECT_FALSE(doc.has_value());
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc,
                         ParseXml("<root><child attr=\"value\"></child>"));
    EXPECT_FALSE(doc.has_value());
  }
}

TEST_F(XmlParserRsTest, DoctypeIgnored) {
  ASSERT_OK_AND_ASSIGN(auto doc,
                       ParseXml("<!DOCTYPE html><html><body /></html>"));
  ASSERT_OK(doc);

  const Node* root = doc->GetRoot();
  ASSERT_EQ(root->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetLocalName(), "html");
  ASSERT_EQ(root->GetChildren().size(), 1u);

  ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kElement);
  EXPECT_EQ(root->GetChildren()[0]->GetLocalName(), "body");
}

TEST_F(XmlParserRsTest, ProcessingInstructionsIgnored) {
  {
    // A top-level processing instruction should be ignored and the actual
    // root element should still be correctly set.
    ASSERT_OK_AND_ASSIGN(auto doc,
                         ParseXml("<?pi content?><root><child/></root>"));
    ASSERT_OK(doc);

    const Node* root = doc->GetRoot();
    ASSERT_EQ(root->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetName(), Name{"root"});
    ASSERT_EQ(root->GetChildren().size(), 1u);

    ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetChildren()[0]->GetName(), Name{"child"});
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc,
                         ParseXml("<root><?pi content?><child/></root>"));
    ASSERT_OK(doc);

    const Node* root = doc->GetRoot();
    ASSERT_EQ(root->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetName(), Name{"root"});
    ASSERT_EQ(root->GetChildren().size(), 1u);

    ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetChildren()[0]->GetName(), Name{"child"});
  }
}

TEST_F(XmlParserRsTest, CommentsIgnored) {
  {
    // A top-level comment should be ignored and the actual root element
    // should still be correctly set.
    ASSERT_OK_AND_ASSIGN(auto doc,
                         ParseXml("<!-- comment --><root><child/></root>"));
    ASSERT_OK(doc);

    const Node* root = doc->GetRoot();
    ASSERT_EQ(root->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetName(), Name{"root"});
    ASSERT_EQ(root->GetChildren().size(), 1u);

    ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetChildren()[0]->GetName(), Name{"child"});
  }

  {
    ASSERT_OK_AND_ASSIGN(auto doc,
                         ParseXml("<root><!-- comment --><child/></root>"));
    ASSERT_OK(doc);

    const Node* root = doc->GetRoot();
    ASSERT_EQ(root->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetName(), Name{"root"});
    ASSERT_EQ(root->GetChildren().size(), 1u);

    ASSERT_EQ(root->GetChildren()[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(root->GetChildren()[0]->GetName(), Name{"child"});
  }
}

}  // namespace

}  // namespace data_decoder::xml
