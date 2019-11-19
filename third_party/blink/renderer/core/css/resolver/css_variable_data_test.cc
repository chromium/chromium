// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

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

}  // namespace blink
