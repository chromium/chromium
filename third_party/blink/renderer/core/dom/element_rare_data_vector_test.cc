// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class TestDataField : public GarbageCollected<TestDataField>,
                      public ElementRareDataField {
 public:
  int value;
};

class ElementRareDataVectorTest : public testing::Test {
 public:
  void SetUp() override {
    rare_data_ = MakeGarbageCollected<ElementRareDataVector>(
        MakeGarbageCollected<NodeRenderingData>(nullptr, nullptr));
  }

  void SetField(unsigned field_id, ElementRareDataField* field) {
    rare_data_->SetField(static_cast<ElementRareDataVector::FieldId>(field_id),
                         field);
  }

  TestDataField* GetField(unsigned field_id) {
    return static_cast<TestDataField*>(rare_data_->GetField(
        static_cast<ElementRareDataVector::FieldId>(field_id)));
  }

  unsigned GetFieldIndex(unsigned field_id) {
    return rare_data_->GetFieldIndex(
        static_cast<ElementRareDataVector::FieldId>(field_id));
  }

  Persistent<ElementRareDataVector> rare_data_;
};

TEST_F(ElementRareDataVectorTest, Basic) {
  TestDataField* d1 = MakeGarbageCollected<TestDataField>();
  d1->value = 101;
  TestDataField* d2 = MakeGarbageCollected<TestDataField>();
  d2->value = 202;

  SetField(1, d1);
  EXPECT_EQ(GetField(1)->value, d1->value);

  SetField(2, d2);
  EXPECT_EQ(GetField(1)->value, d1->value);
  EXPECT_EQ(GetField(2)->value, d2->value);
}

TEST_F(ElementRareDataVectorTest, MutateValue) {
  TestDataField* d1 = MakeGarbageCollected<TestDataField>();
  d1->value = 101;
  TestDataField* d2 = MakeGarbageCollected<TestDataField>();
  d2->value = 202;

  SetField(1, d1);
  EXPECT_EQ(GetField(1)->value, d1->value);

  SetField(1, d2);
  EXPECT_EQ(GetField(1)->value, d2->value);
}

TEST_F(ElementRareDataVectorTest, GetFieldIndex) {
  TestDataField* d1 = MakeGarbageCollected<TestDataField>();
  d1->value = 101;
  TestDataField* d2 = MakeGarbageCollected<TestDataField>();
  d2->value = 202;

  SetField(3, d1);
  EXPECT_EQ(GetFieldIndex(3), 0u);
  EXPECT_EQ(GetField(3)->value, d1->value);

  SetField(5, d2);
  EXPECT_EQ(GetFieldIndex(3), 0u);
  EXPECT_EQ(GetField(3)->value, d1->value);
  EXPECT_EQ(GetFieldIndex(5), 1u);
  EXPECT_EQ(GetField(5)->value, d2->value);
}

}  // namespace blink
