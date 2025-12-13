// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/json/json_values.h"

#include "base/memory/raw_ref.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class JSONValueDeletionVerifier : public JSONValue {
 public:
  explicit JSONValueDeletionVerifier(int& counter) : counter_(counter) {}

  ~JSONValueDeletionVerifier() override { ++(*counter_); }

 private:
  const raw_ref<int> counter_;
};

}  // namespace

TEST(JSONValuesTest, ArrayCastDoesNotLeak) {
  int deletion_count = 0;
  std::unique_ptr<JSONValueDeletionVerifier> not_an_array(
      new JSONValueDeletionVerifier(deletion_count));
  EXPECT_EQ(nullptr, JSONArray::From(std::move(not_an_array)));
  EXPECT_EQ(1, deletion_count);
}

TEST(JSONValuesTest, ObjectCastDoesNotLeak) {
  int deletion_count = 0;
  std::unique_ptr<JSONValueDeletionVerifier> not_an_object(
      new JSONValueDeletionVerifier(deletion_count));
  EXPECT_EQ(nullptr, JSONArray::From(std::move(not_an_object)));
  EXPECT_EQ(1, deletion_count);
}

TEST(JSONValuesTest, ArrayIterator) {
  JSONArray array;
  array.PushString("P");
  array.PushString("A");
  array.PushString("S");
  array.PushString("S");

  StringBuilder result;
  for (const JSONValue& entry : array) {
    String str;
    ASSERT_TRUE(entry.AsString(&str));
    result.Append(str);
  }
  EXPECT_EQ(result.ToString(), "PASS");
}

TEST(JSONValuesTest, EmptyArrayIterator) {
  JSONArray array;

  bool did_iterate = false;
  for (const JSONValue& _ : array) {
    did_iterate = true;
  }
  EXPECT_FALSE(did_iterate);
}

}  // namespace blink
