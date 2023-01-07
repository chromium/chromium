// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_resource_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSResourceValue : public CSSResourceValue {
 public:
  FakeCSSResourceValue(ResourceStatus status) : status_(status) {}
  ResourceStatus Status() const override { return status_; }

  const CSSValue* ToCSSValue() const final { return nullptr; }
  StyleValueType GetType() const final { return kUnknownType; }

 private:
  ResourceStatus status_;
};

}  // namespace

TEST(CSSResourceValueTest, TestStatus) {
  EXPECT_EQ(
      (MakeGarbageCollected<FakeCSSResourceValue>(ResourceStatus::kNotStarted))
          ->state(),
      "unloaded");
  EXPECT_EQ(
      (MakeGarbageCollected<FakeCSSResourceValue>(ResourceStatus::kPending))
          ->state(),
      "loading");
  EXPECT_EQ(
      (MakeGarbageCollected<FakeCSSResourceValue>(ResourceStatus::kCached))
          ->state(),
      "loaded");
  EXPECT_EQ(
      (MakeGarbageCollected<FakeCSSResourceValue>(ResourceStatus::kLoadError))
          ->state(),
      "error");
  EXPECT_EQ(
      (MakeGarbageCollected<FakeCSSResourceValue>(ResourceStatus::kDecodeError))
          ->state(),
      "error");
}

}  // namespace blink
