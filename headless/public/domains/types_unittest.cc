// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/values_test_util.h"
#include "headless/public/devtools/domains/accessibility.h"
#include "headless/public/devtools/domains/dom.h"
#include "headless/public/devtools/domains/memory.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/util/error_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

TEST(TypesTest, IntegerProperty) {
  std::unique_ptr<page::NavigateToHistoryEntryParams> object(
      page::NavigateToHistoryEntryParams::Builder().SetEntryId(123).Build());
  ASSERT_TRUE(object);
  EXPECT_EQ(123, object->GetEntryId());

  std::unique_ptr<page::NavigateToHistoryEntryParams> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ(123, clone->GetEntryId());
}

TEST(TypesTest, IntegerPropertyParseError) {
  const char json[] = "{\"entryId\": \"foo\"}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(page::NavigateToHistoryEntryParams::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, BooleanProperty) {
  std::unique_ptr<memory::SetPressureNotificationsSuppressedParams> object(
      memory::SetPressureNotificationsSuppressedParams::Builder()
          .SetSuppressed(true)
          .Build());
  EXPECT_TRUE(object->GetSuppressed());

  std::unique_ptr<memory::SetPressureNotificationsSuppressedParams> clone(
      object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_TRUE(clone->GetSuppressed());
}

TEST(TypesTest, BooleanPropertyParseError) {
  const char json[] = "{\"suppressed\": \"foo\"}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(
      memory::SetPressureNotificationsSuppressedParams::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, DoubleProperty) {
  std::unique_ptr<page::SetGeolocationOverrideParams> object(
      page::SetGeolocationOverrideParams::Builder().SetLatitude(3.14).Build());
  EXPECT_EQ(3.14, object->GetLatitude());

  std::unique_ptr<page::SetGeolocationOverrideParams> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ(3.14, clone->GetLatitude());
}

TEST(TypesTest, DoublePropertyParseError) {
  const char json[] = "{\"latitude\": \"foo\"}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(page::SetGeolocationOverrideParams::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, StringProperty) {
  std::unique_ptr<page::NavigateParams> object(
      page::NavigateParams::Builder().SetUrl("url").Build());
  EXPECT_EQ("url", object->GetUrl());

  std::unique_ptr<page::NavigateParams> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ("url", clone->GetUrl());
}

TEST(TypesTest, StringPropertyParseError) {
  const char json[] = "{\"url\": false}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(page::NavigateParams::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, EnumProperty) {
  std::unique_ptr<runtime::RemoteObject> object(
      runtime::RemoteObject::Builder()
          .SetType(runtime::RemoteObjectType::UNDEFINED)
          .Build());
  ASSERT_TRUE(object);
  EXPECT_EQ(runtime::RemoteObjectType::UNDEFINED, object->GetType());

  std::unique_ptr<runtime::RemoteObject> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ(runtime::RemoteObjectType::UNDEFINED, clone->GetType());
}

TEST(TypesTest, EnumPropertyParseError) {
  const char json[] = "{\"type\": false}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(runtime::RemoteObject::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, ArrayProperty) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  std::unique_ptr<dom::QuerySelectorAllResult> object(
      dom::QuerySelectorAllResult::Builder().SetNodeIds(values).Build());
  ASSERT_TRUE(object);
  ASSERT_TRUE(object->GetNodeIds());
  const auto& object_node_ids = *object->GetNodeIds();
  ASSERT_EQ(3u, object_node_ids.size());
  EXPECT_EQ(1, object_node_ids[0]);
  EXPECT_EQ(2, object_node_ids[1]);
  EXPECT_EQ(3, object_node_ids[2]);

  std::unique_ptr<dom::QuerySelectorAllResult> clone(object->Clone());
  ASSERT_TRUE(clone);
  ASSERT_TRUE(clone->GetNodeIds());
  const auto& clone_node_ids = *object->GetNodeIds();
  ASSERT_EQ(3u, clone_node_ids.size());
  EXPECT_EQ(1, clone_node_ids[0]);
  EXPECT_EQ(2, clone_node_ids[1]);
  EXPECT_EQ(3, clone_node_ids[2]);
}

TEST(TypesTest, ArrayPropertyParseError) {
  const char json[] = "{\"nodeIds\": true}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(dom::QuerySelectorAllResult::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, ObjectProperty) {
  std::unique_ptr<runtime::RemoteObject> subobject(
      runtime::RemoteObject::Builder()
          .SetType(runtime::RemoteObjectType::SYMBOL)
          .Build());
  std::unique_ptr<runtime::EvaluateResult> object(
      runtime::EvaluateResult::Builder()
          .SetResult(std::move(subobject))
          .Build());
  ASSERT_TRUE(object);
  EXPECT_EQ(runtime::RemoteObjectType::SYMBOL, object->GetResult()->GetType());

  std::unique_ptr<runtime::EvaluateResult> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ(runtime::RemoteObjectType::SYMBOL, clone->GetResult()->GetType());
}

TEST(TypesTest, ObjectPropertyParseError) {
  const char json[] = "{\"result\": 42}";
  base::Value object = base::test::ParseJson(json);

#if DCHECK_IS_ON()
  ErrorReporter errors;
  EXPECT_FALSE(runtime::EvaluateResult::Parse(object, &errors));
  EXPECT_TRUE(errors.HasErrors());
#endif  // DCHECK_IS_ON()
}

TEST(TypesTest, AnyProperty) {
  std::unique_ptr<base::Value> value(new base::Value(123));
  std::unique_ptr<accessibility::AXValue> object(
      accessibility::AXValue::Builder()
          .SetType(accessibility::AXValueType::INTEGER)
          .SetValue(std::move(value))
          .Build());
  ASSERT_TRUE(object);
  EXPECT_EQ(base::Value::Type::INTEGER, object->GetValue()->type());

  std::unique_ptr<accessibility::AXValue> clone(object->Clone());
  ASSERT_TRUE(clone);
  EXPECT_EQ(base::Value::Type::INTEGER, clone->GetValue()->type());

  ASSERT_TRUE(clone->GetValue()->is_int());
  EXPECT_EQ(123, clone->GetValue()->GetInt());
}

TEST(TypesTest, ComplexObjectClone) {
  std::vector<std::unique_ptr<dom::Node>> child_nodes;
  child_nodes.emplace_back(dom::Node::Builder()
                               .SetNodeId(1)
                               .SetBackendNodeId(2)
                               .SetNodeType(3)
                               .SetNodeName("-blink-blink")
                               .SetLocalName("-blink-blink")
                               .SetNodeValue("-blink-blink")
                               .Build());
  std::unique_ptr<dom::SetChildNodesParams> params =
      dom::SetChildNodesParams::Builder()
          .SetParentId(123)
          .SetNodes(std::move(child_nodes))
          .Build();
  std::unique_ptr<dom::SetChildNodesParams> clone = params->Clone();
  ASSERT_TRUE(clone);

  std::string orig;
  JSONStringValueSerializer(&orig).Serialize(base::Value(params->Serialize()));
  std::string clone_value;
  JSONStringValueSerializer(&clone_value)
      .Serialize(base::Value(clone->Serialize()));
  EXPECT_EQ(orig, clone_value);
}

}  // namespace headless
