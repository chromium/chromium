// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

using css_test_helpers::CreateVariableData;

TEST(CSSVariableDataTest, FontUnitsDetected) {
  EXPECT_FALSE(CreateVariableData("100px")->HasFontUnits());
  EXPECT_FALSE(CreateVariableData("10%")->HasFontUnits());
  EXPECT_FALSE(CreateVariableData("10vw")->HasFontUnits());
  EXPECT_FALSE(CreateVariableData("10rem")->HasFontUnits());

  EXPECT_TRUE(CreateVariableData("10em")->HasFontUnits());
  EXPECT_TRUE(CreateVariableData("10ch")->HasFontUnits());
  EXPECT_TRUE(CreateVariableData("10ex")->HasFontUnits());
  EXPECT_TRUE(CreateVariableData("calc(10em + 10%)")->HasFontUnits());
}

TEST(CSSVariableDataTest, RootFontUnitsDetected) {
  EXPECT_FALSE(CreateVariableData("100px")->HasRootFontUnits());
  EXPECT_FALSE(CreateVariableData("10%")->HasRootFontUnits());
  EXPECT_FALSE(CreateVariableData("10vw")->HasRootFontUnits());
  EXPECT_FALSE(CreateVariableData("10em")->HasRootFontUnits());
  EXPECT_FALSE(CreateVariableData("10ch")->HasRootFontUnits());
  EXPECT_FALSE(CreateVariableData("10ex")->HasRootFontUnits());

  EXPECT_TRUE(CreateVariableData("10rem")->HasRootFontUnits());
  EXPECT_TRUE(CreateVariableData("calc(10rem + 10%)")->HasRootFontUnits());
}

TEST(CSSVariableDataTest, Serialize) {
  const String test_cases[] = {
      " /*hello*/", " url(test.svg#a)",
      "\"value\"",  "'value'",
      "a.1",        "5257114e-22df-4378-a8e7-61897860f71e",
      "11111111",
  };

  for (String test_case : test_cases) {
    EXPECT_EQ(CreateVariableData(test_case)->Serialize(), test_case);
  }
}

TEST(CSSVariableDataTest, SerializeSpecialCases) {
  const String replacement_character_string(&kReplacementCharacter, 1u);
  const std::pair<String, String> test_cases[] = {
      {"value\\", "value" + replacement_character_string},
      {"\"value\\", "\"value\""},
      {"url(test.svg\\", "url(test.svg" + replacement_character_string + ")"},
  };

  for (auto test_case : test_cases) {
    EXPECT_EQ(CreateVariableData(test_case.first)->Serialize(),
              test_case.second);
  }
}

}  // namespace blink
