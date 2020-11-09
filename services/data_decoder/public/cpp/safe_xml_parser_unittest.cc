// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_xml_parser.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/data_decoder/xml_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {

std::unique_ptr<base::Value> ParseXml(const std::string& xml) {
  XmlParser parser_impl;
  mojom::XmlParser& parser = parser_impl;
  std::unique_ptr<base::Value> root_node;

  parser.Parse(xml,
               base::BindLambdaForTesting(
                   [&root_node](base::Optional<base::Value> parsed_root_node,
                                const base::Optional<std::string>& error) {
                     root_node = parsed_root_node
                                     ? base::Value::ToUniquePtrValue(
                                           std::move(parsed_root_node.value()))
                                     : nullptr;
                   }));

  return root_node;
}

void ExpectElementTextEq(const base::Value& element,
                         const std::string& expected_text) {
  std::string text;
  if (expected_text.empty()) {
    EXPECT_FALSE(GetXmlElementText(element, &text));
  } else {
    EXPECT_TRUE(GetXmlElementText(element, &text));
    EXPECT_EQ(expected_text, text);
  }
}

using SafeXmlParserTest = testing::Test;

}  // namespace

TEST_F(SafeXmlParserTest, NameAccessors) {
  // Test that the API does not choke on non XML element values.
  base::Value not_an_xml_value;
  EXPECT_FALSE(IsXmlElementNamed(not_an_xml_value, "hello"));
  not_an_xml_value = base::Value("hello");
  EXPECT_FALSE(IsXmlElementNamed(not_an_xml_value, "hello"));

  // Test IsXmlElementNamed.
  std::unique_ptr<base::Value> xml_element = ParseXml("<hello></hello>");
  ASSERT_TRUE(xml_element);
  EXPECT_TRUE(IsXmlElementNamed(*xml_element, "hello"));
  EXPECT_FALSE(IsXmlElementNamed(*xml_element, "bonjour"));

  // Test GetXmlElementTagName.
  std::string tag_name;
  EXPECT_TRUE(GetXmlElementTagName(*xml_element, &tag_name));
  EXPECT_EQ("hello", tag_name);
}

TEST_F(SafeXmlParserTest, TextAccessor) {
  // Test that the API does not choke on non XML element values.
  ExpectElementTextEq(base::Value(), "");
  ExpectElementTextEq(base::Value("hello"), "");

  // Test retrieving text from elements with no text.
  std::unique_ptr<base::Value> no_text_element = ParseXml("<hello/>");
  ExpectElementTextEq(*no_text_element, "");

  // Test retrieving text from elements with actual text.
  std::unique_ptr<base::Value> text_element =
      ParseXml("<hello>bonjour bonjour</hello>");
  ASSERT_TRUE(text_element);
  ExpectElementTextEq(*text_element, "bonjour bonjour");

  // Retrieving text from elements with multiple text children returns the first
  // one only.
  std::unique_ptr<base::Value> multiple_text_elements =
      ParseXml("<hello>bonjour<space/>bonjour</hello>");
  ASSERT_TRUE(multiple_text_elements);
  ExpectElementTextEq(*multiple_text_elements, "bonjour");

  std::unique_ptr<base::Value> cdata_element =
      ParseXml("<hello><![CDATA[This is <b>CData</b>.]]></hello>");
  ASSERT_TRUE(cdata_element);
  ExpectElementTextEq(*cdata_element, "This is <b>CData</b>.");

  std::unique_ptr<base::Value> text_and_cdata_element =
      ParseXml("<hello>This is text.<![CDATA[This is <b>CData</b>.]]></hello>");
  ASSERT_TRUE(text_and_cdata_element);
  ExpectElementTextEq(*text_and_cdata_element, "This is text.");

  std::unique_ptr<base::Value> cdata_and_text_element =
      ParseXml("<hello><![CDATA[This is <b>CData</b>.]]>This is text.</hello>");
  ASSERT_TRUE(cdata_and_text_element);
  ExpectElementTextEq(*cdata_and_text_element, "This is <b>CData</b>.");
}

TEST_F(SafeXmlParserTest, AttributeAccessor) {
  // Test that the API does not choke on non XML element values.
  EXPECT_EQ("", GetXmlElementAttribute(base::Value(), "lang"));
  EXPECT_EQ("", GetXmlElementAttribute(base::Value("hello"), "lang"));

  // No attributes.
  std::unique_ptr<base::Value> element = ParseXml("<hello/>");
  ASSERT_TRUE(element);
  EXPECT_EQ("", GetXmlElementAttribute(*element, "lang"));

  // Wrong and right attributes.
  element = ParseXml("<hello lang='fr' id='123'>bonjour</hello>");
  ASSERT_TRUE(element);
  EXPECT_EQ("", GetXmlElementAttribute(*element, "language"));
  EXPECT_EQ("fr", GetXmlElementAttribute(*element, "lang"));

  // Namespaces.
  element = ParseXml(
      "<hello xmlns:foo='http://foo' foo:lang='fr' lang='es'>bonjour</hello>");
  ASSERT_TRUE(element);
  EXPECT_EQ("es", GetXmlElementAttribute(*element, "lang"));
  EXPECT_EQ("fr", GetXmlElementAttribute(*element, "foo:lang"));
}

TEST_F(SafeXmlParserTest, QualifiedName) {
  EXPECT_EQ(GetXmlQualifiedName("foo", "bar"), "foo:bar");
  EXPECT_EQ(GetXmlQualifiedName("", "foo"), "foo");
}

TEST_F(SafeXmlParserTest, NamespacePrefix) {
  // Fails with no namespace.
  std::unique_ptr<base::Value> no_ns_element = ParseXml("<a/>");
  std::string prefix;
  EXPECT_FALSE(
      GetXmlElementNamespacePrefix(*no_ns_element, "http://a", &prefix));
  EXPECT_TRUE(prefix.empty());
  prefix.clear();

  // Multiple namespaces.
  std::unique_ptr<base::Value> multiple_ns_element = ParseXml(
      "<a xmlns='http://a' xmlns:b='http://b' xmlns:test='http://c'/>");
  EXPECT_TRUE(
      GetXmlElementNamespacePrefix(*multiple_ns_element, "http://a", &prefix));
  EXPECT_TRUE(prefix.empty());  // Default namespace has an empty prefix.
  prefix.clear();
  EXPECT_TRUE(
      GetXmlElementNamespacePrefix(*multiple_ns_element, "http://b", &prefix));
  EXPECT_EQ(prefix, "b");
  prefix.clear();
  EXPECT_TRUE(
      GetXmlElementNamespacePrefix(*multiple_ns_element, "http://c", &prefix));
  EXPECT_EQ(prefix, "test");
  prefix.clear();
  EXPECT_FALSE(
      GetXmlElementNamespacePrefix(*multiple_ns_element, "http://d", &prefix));
  EXPECT_TRUE(prefix.empty());
}

TEST_F(SafeXmlParserTest, ChildAccessor) {
  // Test that the API does not choke on non XML element values.
  base::Value not_an_xml_value;
  EXPECT_FALSE(GetXmlElementChildrenCount(not_an_xml_value, "hello"));
  EXPECT_FALSE(GetXmlElementChildWithTag(not_an_xml_value, "hello"));

  // Childless element case.
  std::unique_ptr<base::Value> childless_element = ParseXml("<hello/>");
  ASSERT_TRUE(childless_element);
  EXPECT_EQ(0, GetXmlElementChildrenCount(*childless_element, "fr"));
  EXPECT_FALSE(GetXmlElementChildWithTag(*childless_element, "fr"));
  std::vector<const base::Value*> children;
  EXPECT_FALSE(
      GetAllXmlElementChildrenWithTag(*childless_element, "fr", &children));
  childless_element = ParseXml("<hello>bonjour</hello>");
  EXPECT_EQ(0, GetXmlElementChildrenCount(*childless_element, "fr"));
  EXPECT_FALSE(GetXmlElementChildWithTag(*childless_element, "fr"));
  EXPECT_FALSE(
      GetAllXmlElementChildrenWithTag(*childless_element, "fr", &children));

  // Element with children case.
  std::unique_ptr<base::Value> element =
      ParseXml("<hello><fr>bonjour</fr><fr>salut</fr><es>hola</es></hello>");
  ASSERT_TRUE(element);
  EXPECT_EQ(2, GetXmlElementChildrenCount(*element, "fr"));
  EXPECT_EQ(1, GetXmlElementChildrenCount(*element, "es"));
  EXPECT_EQ(0, GetXmlElementChildrenCount(*element, "jp"));
  const base::Value* value = GetXmlElementChildWithTag(*element, "fr");
  ASSERT_TRUE(value);
  // The first matching element is returned.
  ExpectElementTextEq(*value, "bonjour");

  EXPECT_TRUE(GetAllXmlElementChildrenWithTag(*element, "fr", &children));
  ASSERT_EQ(children.size(), 2U);
  ExpectElementTextEq(*children[0], "bonjour");
  ExpectElementTextEq(*children[1], "salut");
  children.clear();

  value = GetXmlElementChildWithTag(*element, "es");
  ExpectElementTextEq(*value, "hola");
  EXPECT_TRUE(GetAllXmlElementChildrenWithTag(*element, "es", &children));
  ASSERT_EQ(children.size(), 1U);
  ExpectElementTextEq(*children[0], "hola");

  EXPECT_FALSE(GetXmlElementChildWithTag(*element, "jp"));
  EXPECT_FALSE(GetAllXmlElementChildrenWithTag(*element, "jp", &children));
}

TEST_F(SafeXmlParserTest, FindByPath) {
  // Test that the API does not choke on non XML element values.
  EXPECT_FALSE(
      FindXmlElementPath(base::Value(), {"hello"}, /*unique_path=*/nullptr));

  std::unique_ptr<base::Value> element = ParseXml(
      "<hello>"
      "  <fr>"
      "    <formal>bonjour</formal>"
      "    <casual>salut</casual>"
      "    <casual>ca gaze</casual>"
      "  </fr>"
      "  <es>"
      "    <formal>buenos dias</formal>"
      "    <casual>hola</casual>"
      "  </es>"
      "</hello>");

  // Unexiting paths.
  EXPECT_FALSE(FindXmlElementPath(*element, {"bad"}, /*unique_path=*/nullptr));
  EXPECT_FALSE(
      FindXmlElementPath(*element, {"hello", "bad"}, /*unique_path=*/nullptr));

  // Partial paths.
  const base::Value* fr_element =
      FindXmlElementPath(*element, {"hello", "fr"}, /*unique_path=*/nullptr);
  ASSERT_TRUE(fr_element);
  EXPECT_TRUE(IsXmlElementNamed(*fr_element, "fr"));
  EXPECT_EQ(1, GetXmlElementChildrenCount(*fr_element, "formal"));
  EXPECT_EQ(2, GetXmlElementChildrenCount(*fr_element, "casual"));

  // Path to a leaf element.
  const base::Value* es_element = FindXmlElementPath(
      *element, {"hello", "es", "casual"}, /*unique_path=*/nullptr);
  ASSERT_TRUE(es_element);
  ExpectElementTextEq(*es_element, "hola");

  // Test unique path.
  bool unique_path = true;
  fr_element =
      FindXmlElementPath(*element, {"hello", "fr", "casual"}, &unique_path);
  ASSERT_TRUE(fr_element);
  EXPECT_FALSE(unique_path);

  unique_path = false;
  fr_element =
      FindXmlElementPath(*element, {"hello", "es", "casual"}, &unique_path);
  ASSERT_TRUE(fr_element);
  EXPECT_TRUE(unique_path);
}

}  // namespace data_decoder
