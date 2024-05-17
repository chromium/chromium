// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/transform_view.h"

#include <functional>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::bindings {
namespace {

struct to_string {
  WTF::String operator()(int n) const { return WTF::String::Number(n); }
};

static_assert(std::forward_iterator<
              decltype(Transform<to_string>(std::vector<int>()).begin())>);

TEST(TransformView, Empty) {
  EXPECT_THAT(Transform<std::identity>(std::vector<int>{}), testing::IsEmpty());
  EXPECT_THAT(Transform<std::identity>(std::vector<int>{}), testing::SizeIs(0));
}

TEST(TransformView, Basic) {
  std::vector<int> in{1, 2, 3};
  EXPECT_THAT(Transform<std::identity>(in).size(), testing::Eq(3ul));
  EXPECT_THAT(Transform<std::identity>(in), testing::SizeIs(3));
  EXPECT_THAT(Transform<std::identity>(in), testing::ElementsAre(1, 2, 3));
  EXPECT_THAT(Transform<std::negate<int>>(in),
              testing::ElementsAre(-1, -2, -3));
  static int arr[] = {5, 6, 7};
  EXPECT_THAT(Transform<std::negate<int>>(arr),
              testing::ElementsAre(-5, -6, -7));
}

TEST(TransformView, DifferentType) {
  std::vector<int> in{1, 2, 3};
  EXPECT_THAT(Transform<to_string>(in), testing::ElementsAre("1", "2", "3"));
}

TEST(TransformView, NonRandomAccessIterator) {
  std::set<int> in{1, 2, 3};
  EXPECT_THAT(Transform<std::negate<int>>(in),
              testing::UnorderedElementsAre(-1, -2, -3));
}

}  // namespace
}  // namespace blink::bindings
