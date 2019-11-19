// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

TEST(FontSelectionTypesTest, HashCollisions) {
  Vector<int> weights = {100, 200, 300, 400, 500, 600, 700, 800, 900};
  Vector<float> slopes = {-90, -67.5, -30, -20, -10, 0, 10, 20, 30, 67.5, 90};
  Vector<float> widths = {50, 67.5, 75, 100, 125, 150, 167.5, 175, 200};

  HashSet<unsigned> hashes;
  for (auto weight : weights) {
    for (auto slope : slopes) {
      for (auto width : widths) {
        FontSelectionRequest request = FontSelectionRequest(
            FontSelectionValue(weight), FontSelectionValue(width),
            FontSelectionValue(slope));
        ASSERT_FALSE(hashes.Contains(request.GetHash()));
        ASSERT_TRUE(hashes.insert(request.GetHash()).is_new_entry);
      }
    }
  }
  ASSERT_EQ(hashes.size(), weights.size() * slopes.size() * widths.size());
}

TEST(FontSelectionTypesTest, ValueToString) {
  {
    FontSelectionValue value(42);
    EXPECT_EQ("42.000000", value.ToString());
  }
  {
    FontSelectionValue value(42.81f);
    EXPECT_EQ("42.750000", value.ToString());
  }
  {
    FontSelectionValue value(42.923456789123456789);
    EXPECT_EQ("42.750000", value.ToString());
  }
}

TEST(FontSelectionTypesTest, RequestToString) {
  FontSelectionRequest request(FontSelectionValue(42), FontSelectionValue(43),
                               FontSelectionValue(44));
  EXPECT_EQ("weight=42.000000, width=43.000000, slope=44.000000",
            request.ToString());
}

}  // namespace blink
