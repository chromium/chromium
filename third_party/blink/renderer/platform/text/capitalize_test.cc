// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/capitalize.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct CapitalizeTestData {
  String input;
  String expected;
  UChar previous_character = kSpaceCharacter;
};

class CapitalizeTest : public testing::Test,
                       public testing::WithParamInterface<CapitalizeTestData> {
};

INSTANTIATE_TEST_SUITE_P(CapitalizeTest,
                         CapitalizeTest,
                         testing::Values(CapitalizeTestData{String(), String()},
                                         CapitalizeTestData{"", ""},
                                         CapitalizeTestData{"hello, world",
                                                            "Hello, World"}));

TEST_P(CapitalizeTest, Data) {
  const auto& data = GetParam();
  EXPECT_EQ(data.expected, Capitalize(data.input, data.previous_character));
}

}  // namespace blink
