// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/capitalize.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

struct CapitalizeTestData {
  String input;
  String expected;
  UChar32 previous_character = uchar::kSpace;
  bool is_supplementary_character_fix_enabled = true;
};

class CapitalizeTest : public testing::Test,
                       public testing::WithParamInterface<CapitalizeTestData> {
};

INSTANTIATE_TEST_SUITE_P(
    CapitalizeTest,
    CapitalizeTest,
    testing::Values(
        CapitalizeTestData{String(), String()},
        CapitalizeTestData{String(), String(), uchar::kSpace, false},
        CapitalizeTestData{"", ""},
        CapitalizeTestData{"", "", uchar::kSpace, false},
        CapitalizeTestData{"hello, world", "Hello, World"},
        CapitalizeTestData{"hello, world", "Hello, World", uchar::kSpace,
                           false},
        CapitalizeTestData{"bc def", "bc Def", uchar::kMathBoldUpperA}));

TEST_P(CapitalizeTest, Data) {
  const auto& data = GetParam();
  ScopedCapitalizeAfterSupplementaryCharacterFixForTest scoped_fix(
      data.is_supplementary_character_fix_enabled);
  EXPECT_EQ(data.expected, Capitalize(data.input, data.previous_character));
}

}  // namespace blink
