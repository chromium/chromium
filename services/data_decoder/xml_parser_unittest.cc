// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "services/data_decoder/xml_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/files/file_util.h"

namespace data_decoder {

namespace {

void TestParseXmlCallback(std::unique_ptr<base::Value>* value_out,
                          base::Optional<std::string>* error_out,
                          base::Optional<base::Value> value,
                          const base::Optional<std::string>& error) {
  std::unique_ptr<base::Value> value_ptr =
      value ? base::Value::ToUniquePtrValue(std::move(value.value())) : nullptr;
  *value_out = std::move(value_ptr);
  *error_out = error;
}

// Parses the passed in |xml| and compares the result to |json|.
// If |json| is empty, the parsing is expected to fail.
void TestParseXml(const std::string& xml, const std::string& json) {
  XmlParser parser_impl;
  // Use a reference to mojom::XmlParser as XmlParser implements the interface
  // privately.
  mojom::XmlParser& parser = parser_impl;

  std::unique_ptr<base::Value> actual_value;
  base::Optional<std::string> error;
  parser.Parse(xml, base::Bind(&TestParseXmlCallback, &actual_value, &error));
  if (json.empty()) {
    EXPECT_TRUE(error);
    EXPECT_FALSE(actual_value)
        << "Unexpected success, value: " << *actual_value;
    return;
  }

  EXPECT_FALSE(error) << "Unexpected error: " << *error;
  EXPECT_TRUE(actual_value);

  std::unique_ptr<base::Value> expected_value =
      base::JSONReader::ReadDeprecated(json);
  DCHECK(expected_value) << "Bad test, incorrect JSON: " << json;

  EXPECT_EQ(*expected_value, *actual_value);
}

}  // namespace

using XmlParserTest = testing::Test;

TEST_F(XmlParserTest, ParseBadXml) {
  std::string invalid_xml_strings[] = {"",
                                       "  ",
                                       "Awesome possum",
                                       R"( ["json", "or", "xml?"] )",
                                       "<unbalanced>",
                                       "<hello>bad tag</goodbye>"};
  for (auto& xml : invalid_xml_strings)
    TestParseXml(xml, "");
}

TEST_F(XmlParserTest, IgnoreComments) {
  TestParseXml("<!-- This is the best XML document IN THE WORLD! --><a></a>",
               R"( {"type": "element", "tag": "a"} )");
}

TEST_F(XmlParserTest, IgnoreProcessingCommands) {
  TestParseXml(R"(<?xml-stylesheet href="mystyle.css" type="text/css"?>
                  <a></a>)",
               R"( {"type": "element", "tag": "a"} )");
  TestParseXml("<a/><?hello?>", R"( {"type": "element", "tag": "a"} )");
}

TEST_F(XmlParserTest, IgnoreDocumentTypes) {
  TestParseXml(
      R"(<?xml version="1.0" encoding="utf-8"?>
         <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
             "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
         <html xmlns="http://www.w3.org/1999/xhtml">Some HTML</html>
      )",
      R"( {"type": "element",
           "namespaces": {"": "http://www.w3.org/1999/xhtml"},
           "tag": "html",
           "children":[{"type": "text", "text": "Some HTML"}]
          }
      )");
}

TEST_F(XmlParserTest, ParseSelfClosingTag) {
  TestParseXml("<a/>", R"( {"type": "element", "tag": "a"} )");
  TestParseXml("<a><b/></a>",
               R"( {"type": "element",
                    "tag": "a",
                    "children": [{"type": "element", "tag": "b"}]} )");
  TestParseXml("<a><b/><b/><b/></a>",
               R"( {"type": "element",
                    "tag": "a",
                    "children":[
                      {"type": "element", "tag": "b"},
                      {"type": "element", "tag": "b"},
                      {"type": "element", "tag": "b"}
                     ]}
                )");
}

TEST_F(XmlParserTest, ParseEmptyTag) {
  TestParseXml("<a></a>", R"( {"type": "element", "tag": "a"} )");
  TestParseXml("<a><b></b></a>",
               R"( {"type": "element",
                    "tag": "a",
                    "children": [{"type": "element", "tag": "b"}]} )");
  TestParseXml("<a><b></b><b></b></a>",
               R"( {"type": "element",
                    "tag": "a",
                    "children":[{"type": "element", "tag": "b"},
                                {"type": "element", "tag": "b"}]} )");
}

TEST_F(XmlParserTest, ParseBadTag) {
  // Parse a tag with an invalid character.
  TestParseXml("<hello\xD8\x00>bonjour</hello\xD8\x00>", "");
  TestParseXml("<hello\xc0\x8a>bonjour</hello\xc0\x8a>", "");
}

TEST_F(XmlParserTest, ParseTextElement) {
  TestParseXml("<hello>bonjour</hello>",
               R"( {"type": "element",
                    "tag": "hello",
                    "children": [{"type": "text", "text": "bonjour"}]} )");
}

TEST_F(XmlParserTest, ParseBadTextElement) {
  TestParseXml("<hello>\xed\xa0\x80\xed\xbf\xbf></hello>", "");
  TestParseXml("<hello>\xc0\x8a</hello>", "");
}

TEST_F(XmlParserTest, ParseCDataElement) {
  TestParseXml(R"(<hello><![CDATA[This is CData.
    With weird chars [ ] { } <> ; : ' " and
    some <b>formatting</b> <br>]]>
                  </hello> )",
               R"( {"type": "element",
                    "tag": "hello",
                    "children": [{"type": "cdata",
                                  "text": "This is CData.\n    With weird chars [ ] { } <> ; : ' \" and\n    some <b>formatting</b> <br>"
                          }]} )");
}

TEST_F(XmlParserTest, ParseBadCDataElement) {
  // Missing first bracket.
  TestParseXml("<hello><!CDATA[This is CData.]]></hello>", "");
  // Space before last bracket.
  TestParseXml("<hello><![CDATA[This is CData.] ]></hello>", "");
  // Space before closing >.
  TestParseXml("<hello><![CDATA[This is CData.]] ></hello>", "");
  // Invalid UTF-8.
  TestParseXml("<a><![CDATA[\xc0\x8a]]><d>", "");
  TestParseXml("<hello><![CDATA[\xed\xa0\x80\xed\xbf\xbf]]></hello>", "");
}

TEST_F(XmlParserTest, ParseTextWithEntities) {
  TestParseXml("<hello>&quot;bonjour&amp; &apos; &lt;hello&gt;</hello>",
               R"( {"type": "element",
                    "tag": "hello",
                    "children": [{"type": "text",
                                  "text": "\"bonjour& ' <hello>"}]} )");
  // Parse invalid UTF8 entities.
  TestParseXml("<hello>&#xD800;</hello>", "");
  // Entities in CDATA are not evaluated.
  TestParseXml("<hello><![CDATA[&quot;bonjour&amp; &apos;]]></hello>",
               R"( {"type": "element",
                    "tag": "hello",
                    "children": [{"type": "cdata",
                                  "text": "&quot;bonjour&amp; &apos;"}]} )");
}

TEST_F(XmlParserTest, ParseMultipleSimilarTextElement) {
  TestParseXml("<hello><fr>bonjour</fr><fr>salut</fr><fr>coucou</fr></hello>",
               R"( {"type": "element",
                    "tag": "hello",
                    "children": [
                      {"type": "element",
                       "tag": "fr",
                       "children": [{"type": "text", "text": "bonjour"}]},
                      {"type": "element",
                       "tag": "fr",
                       "children": [{"type": "text", "text": "salut"}]},
                      {"type": "element",
                       "tag": "fr",
                       "children": [{"type": "text", "text": "coucou"}]}
                    ]} )");
}

TEST_F(XmlParserTest, ParseMixMatchTextNonTextElement) {
  TestParseXml(
      R"(
      <hello>
        <fr>coucou</fr>
        <fr><proper>bonjour</proper><slang>salut</slang></fr>
         <fr>ca va</fr>
      </hello> )",
      R"(
        {"type": "element",
         "tag": "hello",
         "children": [
           {"type": "element",
            "tag": "fr",
            "children": [{"type": "text", "text": "coucou"}]},
           {"type": "element",
            "tag": "fr",
            "children": [
               {"type": "element",
                "tag": "proper",
                "children": [{"type": "text", "text": "bonjour" }]},
               {"type": "element",
                "tag": "slang",
                "children": [{"type": "text", "text": "salut" }]}
           ]},
           {"type": "element",
            "tag": "fr",
            "children": [{"type": "text", "text": "ca va"}]}
         ]} )");
}

TEST_F(XmlParserTest, ParseElementsInText) {
  TestParseXml(
      "<p>This is <b>some</b> text.<![CDATA[ this <i>formatting</i> is ignored"
      " ]]></p>",
      R"(
        {"type": "element", "tag": "p", "children": [
          {"type": "text", "text": "This is "},
          {"type": "element", "tag": "b", "children": [
            {"type": "text", "text": "some"}
          ]},
          {"type": "text", "text": " text."},
          {"type": "cdata", "text": " this <i>formatting</i> is ignored "}
        ]} )");
}

TEST_F(XmlParserTest, ParseNestedXml) {
  TestParseXml(
      R"( <M><a><t><r><y><o><s><h><k><a>Zdravstvuy</a>
          </k></h></s></o></y></r></t></a></M> )",
      R"( {"type": "element", "tag": "M", "children": [
            {"type": "element", "tag": "a", "children": [
              {"type": "element", "tag": "t", "children": [
                {"type": "element", "tag": "r", "children": [
                  {"type": "element", "tag": "y", "children": [
                    {"type": "element", "tag": "o", "children": [
                      {"type": "element", "tag": "s", "children": [
                        {"type": "element", "tag": "h", "children": [
                          {"type": "element", "tag": "k", "children": [
                            {"type": "element", "tag": "a", "children": [
                              {"type": "text", "text": "Zdravstvuy"}
                            ]}
                          ]}
                        ]}
                      ]}
                    ]}
                  ]}
                ]}
              ]}
            ]}
          ]} )");
}

TEST_F(XmlParserTest, ParseMultipleSimilarElements) {
  TestParseXml("<a><b>b1</b><c>c1</c><b>b2</b><c>c2</c><b>b3</b><c>c3</c></a>",
               R"( {"type": "element", "tag": "a", "children": [
                     {"type": "element", "tag": "b", "children":[
                       {"type": "text", "text": "b1"}]},
                     {"type": "element", "tag": "c", "children":[
                        {"type": "text", "text": "c1"}]},
                     {"type": "element", "tag": "b", "children":[
                       {"type": "text", "text": "b2"}]},
                     {"type": "element", "tag": "c",  "children":[
                       {"type": "text", "text": "c2"}]},
                     {"type": "element", "tag": "b",  "children":[
                       {"type": "text", "text": "b3"}]},
                     {"type": "element", "tag": "c",  "children":[
                       {"type": "text", "text": "c3"}]}
                   ]} )");
}

TEST_F(XmlParserTest, LargeNumber) {
  TestParseXml("<number>18446744073709551616</number>",
               R"( {"type": "element",
                    "tag": "number",
                    "children": [
                      { "type": "text", "text": "18446744073709551616"}
                    ]} )");
}

TEST_F(XmlParserTest, JsonInjection) {
  TestParseXml(
      R"( <test>{"tag": "woop", "boing": 123, 12: ""foodyums"}</test> )",
      R"( {"type": "element",
           "tag": "test",
           "children": [
             {"type": "text",
              "text": "{\"tag\": \"woop\", \"boing\": 123, 12: \"\"foodyums\"}"}
           ]}
        )");
}

TEST_F(XmlParserTest, ParseAttributes) {
  TestParseXml("<a b='c'/>",
               R"( {"type": "element",
                    "tag": "a",
                    "attributes": {"b": "c"}} )");
  // Duplicate attributes are considered an error by libxml.
  TestParseXml("<a b='c' b='d'/>", "");
  TestParseXml("<a><b c='d'/></a>",
               R"( {"type": "element",
                    "tag": "a",
                    "children": [
                      {"type": "element", "tag": "b",
                       "attributes":{"c": "d"}}]} )");
  TestParseXml("<hello lang='fr'>bonjour</hello>",
               R"( {"type": "element", "tag": "hello",
                    "attributes": {"lang": "fr"},
                    "children": [{"type": "text", "text": "bonjour"}]
                   } )");
  TestParseXml(
      "<translate lang='fr' id='123'><hello>bonjour</hello></translate>",
      R"( {"type": "element",
           "tag": "translate",
           "attributes": {"lang": "fr", "id": "123"},
           "children": [
               {"type": "element", "tag": "hello",
                "children": [{"type": "text", "text": "bonjour"}]
               }
           ]}
        )");
}

TEST_F(XmlParserTest, ParseBadAttributes) {
  // Key with invalid UTF8.
  TestParseXml("<a b\xc0\x8a='c'/>", "");
  // Value with invalid UTF8.
  TestParseXml("<a b='c\xc0\x8a'/>", "");
}

TEST_F(XmlParserTest, MultipleNamespacesDefined) {
  TestParseXml(
      "<a xmlns='http://a' xmlns:foo='http://foo' xmlns:bar='http://bar'></a>",
      R"( {"type": "element",
           "tag": "a",
           "namespaces":{
             "": "http://a",
             "foo": "http://foo",
             "bar": "http://bar"
          }} )");
}

TEST_F(XmlParserTest, NamespacesUsed) {
  TestParseXml(
      "<foo:a xmlns:foo='http://foo'>"
      "  <foo:b att1='fooless' foo:att2='fooful'>With foo</foo:b>"
      "  <b foo:att1='fooful' att2='fooless'>No foo</b>"
      "</foo:a>",
      R"(
       {"type": "element",
        "tag": "foo:a",
        "namespaces": {"foo": "http://foo"},
        "children": [
          {"type": "element",
           "tag": "foo:b",
           "attributes": {"att1": "fooless", "foo:att2": "fooful"},
           "children": [{"type": "text", "text": "With foo"}]
          },
          {"type": "element",
           "tag": "b",
           "attributes": {"foo:att1": "fooful", "att2": "fooless"},
           "children": [{"type": "text", "text": "No foo"}]
          }
        ]
       } )");
}

TEST_F(XmlParserTest, ParseTypicalXml) {
  constexpr char kXml[] = R"(<?xml version='1.0' encoding='UTF-8'?>
      <!-- This is an XML sample -->
      <library xmlns='http://library' xmlns:foo='http://foo.com'>
        <book foo:id="k123">
          <author>Isaac Newton</author>
          <title>Philosophiae Naturalis Principia Mathematica</title>
          <genre>Science</genre>
          <price>40.95</price>
          <publish_date>1947-9-03</publish_date>
        </book>
        <book foo:id="k456">
          <author>Dr. Seuss</author>
          <title>Green Eggs and Ham</title>
          <genre>Kid</genre>
          <foo:kids/>
          <price>4.95</price>
          <publish_date>1960-8-12</publish_date>
        </book>
      </library>
      )";

  constexpr char kJson[] = R"(
      {"type": "element",
       "tag": "library",
       "namespaces": {
          "": "http://library",
          "foo": "http://foo.com"
        },
       "children": [
        {"type": "element",
         "tag": "book",
         "attributes": {"foo:id": "k123"},
         "children": [
            {"type": "element",
             "tag": "author",
             "children": [{"type": "text", "text": "Isaac Newton"}]
            },
            {"type": "element",
             "tag": "title",
             "children": [
               {"type": "text",
                "text": "Philosophiae Naturalis Principia Mathematica"}
             ]
            },
            {"type": "element",
             "tag": "genre",
             "children": [{"type": "text", "text": "Science"}]
            },
            {"type": "element",
             "tag": "price",
             "children": [{"type": "text", "text": "40.95"}]
            },
            {"type": "element",
             "tag": "publish_date",
             "children": [{"type": "text", "text": "1947-9-03"}]
            }
          ]
        },
        {"type": "element",
         "tag": "book",
         "attributes": {"foo:id": "k456"},
         "children": [
           {"type": "element",
            "tag": "author",
            "children": [{"type": "text", "text": "Dr. Seuss"}]
           },
           {"type": "element",
            "tag": "title",
            "children": [{"type": "text", "text": "Green Eggs and Ham"}]
           },
           {"type": "element",
            "tag": "genre",
            "children": [{"type": "text", "text": "Kid"}]
           },
           {"type": "element",
            "tag": "foo:kids"
           },
           {"type": "element",
            "tag": "price",
            "children": [{"type": "text", "text": "4.95"}]
           },
           {"type": "element",
            "tag": "publish_date",
            "children": [{"type": "text", "text": "1960-8-12"}]
           }
        ]}
        ]
      }
      )";
  TestParseXml(kXml, kJson);
}

}  // namespace data_decoder
