// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/copyless_paste_extractor.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/document_metadata/copyless_paste.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

using mojom::document_metadata::blink::Entity;
using mojom::document_metadata::blink::EntityPtr;
using mojom::document_metadata::blink::Property;
using mojom::document_metadata::blink::PropertyPtr;
using mojom::document_metadata::blink::Values;
using mojom::document_metadata::blink::ValuesPtr;
using mojom::document_metadata::blink::WebPage;
using mojom::document_metadata::blink::WebPagePtr;

class CopylessPasteExtractorTest : public PageTestBase {
 public:
  CopylessPasteExtractorTest() = default;

 protected:
  void TearDown() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  WebPagePtr Extract() {
    return CopylessPasteExtractor::Extract(GetDocument());
  }

  void SetHTMLInnerHTML(const String&);

  void SetURL(const String&);

  void SetTitle(const String&);

  PropertyPtr CreateStringProperty(const String& name, const String& value);

  PropertyPtr CreateBooleanProperty(const String& name, const bool& value);

  PropertyPtr CreateLongProperty(const String& name, const int64_t& value);

  PropertyPtr CreateEntityProperty(const String& name, EntityPtr value);

  WebPagePtr CreateWebPage(const String& url, const String& title);
};

void CopylessPasteExtractorTest::SetHTMLInnerHTML(const String& html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString((html_content));
}

void CopylessPasteExtractorTest::SetURL(const String& url) {
  GetDocument().SetURL(blink::KURL(url));
}

void CopylessPasteExtractorTest::SetTitle(const String& title) {
  GetDocument().setTitle(title);
}

PropertyPtr CopylessPasteExtractorTest::CreateStringProperty(
    const String& name,
    const String& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_string_values({value});
  return property;
}

PropertyPtr CopylessPasteExtractorTest::CreateBooleanProperty(
    const String& name,
    const bool& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_bool_values({value});
  return property;
}

PropertyPtr CopylessPasteExtractorTest::CreateLongProperty(
    const String& name,
    const int64_t& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_long_values({value});
  return property;
}

PropertyPtr CopylessPasteExtractorTest::CreateEntityProperty(const String& name,
                                                             EntityPtr value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_entity_values(Vector<EntityPtr>());
  property->values->get_entity_values().push_back(std::move(value));
  return property;
}

WebPagePtr CopylessPasteExtractorTest::CreateWebPage(const String& url,
                                                     const String& title) {
  WebPagePtr page = WebPage::New();
  page->url = blink::KURL(url);
  page->title = title;
  return page;
}

TEST_F(CopylessPasteExtractorTest, empty) {
  ASSERT_TRUE(Extract().is_null());
}

TEST_F(CopylessPasteExtractorTest, basic) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Special characters for ya >_<;"));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, header) {
  SetHTMLInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</head>");

  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Special characters for ya >_<;"));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, booleanValue) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"open\": true"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(CreateBooleanProperty("open", true));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, longValue) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"long\": 1"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(CreateLongProperty("long", 1ll));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, doubleValue) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"double\": 1.5"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(CreateStringProperty("double", "1.5"));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, multiple) {
  SetHTMLInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</head>"
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</body>");

  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  for (int i = 0; i < 3; ++i) {
    EntityPtr restaurant = Entity::New();
    restaurant->type = "Restaurant";
    restaurant->properties.push_back(
        CreateStringProperty("name", "Special characters for ya >_<;"));

    expected->entities.push_back(std::move(restaurant));
  }
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, nested) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Ye ol greasy diner\","
      "\"address\": {"
      "\n"
      "  \"streetAddress\": \"123 Big Oak Road\","
      "  \"addressLocality\": \"San Francisco\""
      "  }\n"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Ye ol greasy diner"));

  EntityPtr address = Entity::New();
  address->type = "Thing";
  address->properties.push_back(
      CreateStringProperty("streetAddress", "123 Big Oak Road"));
  address->properties.push_back(
      CreateStringProperty("addressLocality", "San Francisco"));

  restaurant->properties.push_back(
      CreateEntityProperty("address", std::move(address)));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, repeated) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": [ \"First name\", \"Second name\" ]"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  Vector<String> nameValues;
  nameValues.push_back("First name");
  nameValues.push_back("Second name");
  name->values->set_string_values(nameValues);

  restaurant->properties.push_back(std::move(name));

  expected->entities.push_back(std::move(restaurant));

  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, repeatedObject) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Ye ol greasy diner\","
      "\"address\": ["
      "\n"
      "  {"
      "  \"streetAddress\": \"123 Big Oak Road\","
      "  \"addressLocality\": \"San Francisco\""
      "  },\n"
      "  {"
      "  \"streetAddress\": \"123 Big Oak Road\","
      "  \"addressLocality\": \"San Francisco\""
      "  }\n"
      "]\n"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Ye ol greasy diner"));

  PropertyPtr addressProperty = Property::New();
  addressProperty->name = "address";
  addressProperty->values = Values::New();
  addressProperty->values->set_entity_values(Vector<EntityPtr>());
  for (int i = 0; i < 2; ++i) {
    EntityPtr address = Entity::New();
    address->type = "Thing";
    address->properties.push_back(
        CreateStringProperty("streetAddress", "123 Big Oak Road"));
    address->properties.push_back(
        CreateStringProperty("addressLocality", "San Francisco"));
    addressProperty->values->get_entity_values().push_back(std::move(address));
  }
  restaurant->properties.push_back(std::move(addressProperty));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, truncateLongString) {
  StringBuilder maxLengthString;
  for (int i = 0; i < 200; ++i) {
    maxLengthString.Append("a");
  }
  StringBuilder tooLongString;
  tooLongString.Append(maxLengthString);
  tooLongString.Append("a");
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"" +
      tooLongString.ToString() +
      "\""
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", maxLengthString.ToString()));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, enforceTypeExists) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(CopylessPasteExtractorTest, UnhandledTypeIgnored) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"UnsupportedType\","
      "\"name\": \"Special characters for ya >_<;\""
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(CopylessPasteExtractorTest, truncateTooManyValuesInField) {
  StringBuilder largeRepeatedField;
  largeRepeatedField.Append("[");
  for (int i = 0; i < 101; ++i) {
    largeRepeatedField.Append("\"a\"");
    if (i != 100) {
      largeRepeatedField.Append(", ");
    }
  }
  largeRepeatedField.Append("]");
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": " +
      largeRepeatedField.ToString() +
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  Vector<String> nameValues;
  for (int i = 0; i < 100; ++i) {
    nameValues.push_back("a");
  }
  name->values->set_string_values(nameValues);

  restaurant->properties.push_back(std::move(name));

  expected->entities.push_back(std::move(restaurant));

  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, truncateTooManyFields) {
  StringBuilder tooManyFields;
  for (int i = 0; i < 20; ++i) {
    tooManyFields.AppendFormat("\"%d\": \"a\"", i);
    if (i != 19) {
      tooManyFields.Append(",\n");
    }
  }
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\"," +
      tooManyFields.ToString() +
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  for (int i = 0; i < 19; ++i) {
    restaurant->properties.push_back(
        CreateStringProperty(String::Number(i), "a"));
  }

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, ignorePropertyWithEmptyArray) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": []"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  expected->entities.push_back(std::move(restaurant));

  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, ignorePropertyWithMixedTypes) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": [ \"Name\", 1 ]"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  expected->entities.push_back(std::move(restaurant));

  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, ignorePropertyWithNestedArray) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": [ [ \"Name\" ] ]"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";

  expected->entities.push_back(std::move(restaurant));

  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, enforceMaxNestingDepth) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Ye ol greasy diner\","
      "\"1\": {"
      "  \"2\": {"
      "    \"3\": {"
      "      \"4\": {"
      "        \"5\": 6"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Ye ol greasy diner"));

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";

  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";

  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";

  entity2->properties.push_back(CreateEntityProperty("3", std::move(entity3)));

  entity1->properties.push_back(CreateEntityProperty("2", std::move(entity2)));

  restaurant->properties.push_back(
      CreateEntityProperty("1", std::move(entity1)));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, maxNestingDepthWithTerminalProperty) {
  SetHTMLInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">"
      "\n"
      "\n"
      "{\"@type\": \"Restaurant\","
      "\"name\": \"Ye ol greasy diner\","
      "\"1\": {"
      "  \"2\": {"
      "    \"3\": {"
      "      \"4\": 5"
      "    }\n"
      "  }\n"
      "}\n"
      "}\n"
      "\n"
      "</script>"
      "</body>");
  SetURL("http://www.test.com/");
  SetTitle("My neat website about cool stuff");

  WebPagePtr extracted = Extract();
  ASSERT_FALSE(extracted.is_null());

  WebPagePtr expected =
      CreateWebPage("http://www.test.com/", "My neat website about cool stuff");

  EntityPtr restaurant = Entity::New();
  restaurant->type = "Restaurant";
  restaurant->properties.push_back(
      CreateStringProperty("name", "Ye ol greasy diner"));

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";

  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";

  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";

  entity3->properties.push_back(CreateLongProperty("4", 5));

  entity2->properties.push_back(CreateEntityProperty("3", std::move(entity3)));

  entity1->properties.push_back(CreateEntityProperty("2", std::move(entity2)));

  restaurant->properties.push_back(
      CreateEntityProperty("1", std::move(entity1)));

  expected->entities.push_back(std::move(restaurant));
  EXPECT_EQ(expected, extracted);
}

}  // namespace
}  // namespace blink
