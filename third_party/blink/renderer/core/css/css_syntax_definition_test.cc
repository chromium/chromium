// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
namespace blink {

class CSSSyntaxDefinitionTest : public testing::Test {};

const char* kValidSyntaxStr[] = {"<number>+",
                                 "<length> | <percentage>#",
                                 "ident | <angle>+ | ident#",
                                 "<time> | time",
                                 "<angle>",
                                 "*"};

class CSSSyntaxDefinitionFromStringTest
    : public CSSSyntaxDefinitionTest,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTest,
                         CSSSyntaxDefinitionFromStringTest,
                         testing::ValuesIn(kValidSyntaxStr));

TEST_P(CSSSyntaxDefinitionFromStringTest, TestToString) {
  String syntax_str(GetParam());
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxStringParser(syntax_str).Parse();
  DCHECK(syntax.has_value());
  Vector<CSSSyntaxComponent> components = syntax->Components();
  EXPECT_EQ(syntax->ToString(), syntax_str);
}

}  // namespace blink
