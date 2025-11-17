// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

constexpr const char* kUniversalStr[] = {"*", "* ", "*\r\n", "*\f",
                                         "*\n\t\r\f"};

constexpr const char* kValidSyntaxStr[] = {"<number>+",
                                           "<length> | <percentage>#",
                                           "ident | <angle>+ | ident#",
                                           "<time> | time",
                                           "<angle>",
                                           "<number>",
                                           "ident"};
constexpr const char* kInvalidSyntaxStr[] = {
    "",  "<transform-list>+", "[abc]", ")",        "<abc>", "<abc",
    "+", "< number>",         "! ",    "<number >"};

class CSSSyntaxDefinitionTest : public testing::Test {
 public:
  CSSSyntaxDefinition CreateUniversalDescriptor() {
    return CSSSyntaxDefinition::CreateUniversal();
  }
};

class CSSSyntaxDefinitionFromStringTest
    : public CSSSyntaxDefinitionTest,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTest,
                         CSSSyntaxDefinitionFromStringTest,
                         testing::ValuesIn(kValidSyntaxStr));

TEST_P(CSSSyntaxDefinitionFromStringTest, StringParserToString) {
  String syntax_str(GetParam());
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxStringParser(syntax_str).Parse();
  DCHECK(syntax.has_value());
  Vector<CSSSyntaxComponent> components = syntax->Components();
  EXPECT_EQ(syntax->ToString(), syntax_str);
}

TEST_P(CSSSyntaxDefinitionFromStringTest, StreamParserToString) {
  String syntax_str(GetParam());
  CSSParserTokenStream stream(syntax_str);
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());
  Vector<CSSSyntaxComponent> components = syntax->Components();
  EXPECT_EQ(syntax->ToString(), syntax_str);
}

class UniversalSyntaxTest : public CSSSyntaxDefinitionTest,
                            public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTest,
                         UniversalSyntaxTest,
                         testing::ValuesIn(kUniversalStr));

TEST_P(UniversalSyntaxTest, TestSimple) {
  auto universal = CreateUniversalDescriptor();
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(*syntax, universal);
}

class UniversalSyntaxStreamOffsetTest
    : public CSSSyntaxDefinitionTest,
      public testing::WithParamInterface<std::tuple<const char*, const char*>> {
};

INSTANTIATE_TEST_SUITE_P(
    CSSSyntaxDefinitionTest,
    UniversalSyntaxStreamOffsetTest,
    testing::Combine(testing::ValuesIn(kUniversalStr),
                     testing::ValuesIn(kInvalidSyntaxStr)));

TEST_P(UniversalSyntaxStreamOffsetTest, TestStreamOffsetAfterConsuming) {
  auto [valid_syntax, invalid_syntax] = GetParam();
  String valid_syntax_str(valid_syntax);
  String invalid_syntax_str(invalid_syntax);
  CSSParserTokenStream valid_syntax_stream(valid_syntax_str);
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(valid_syntax_stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(valid_syntax_stream.Offset(), valid_syntax_str.length());

  CSSParserTokenStream invalid_syntax_stream(invalid_syntax_str);
  syntax = CSSSyntaxDefinition::Consume(invalid_syntax_stream);
  ASSERT_FALSE(syntax.has_value());
  EXPECT_EQ(invalid_syntax_stream.Offset(), 0u);

  String syntax_str_with_separator =
      valid_syntax_str + " | " + invalid_syntax_str;
  CSSParserTokenStream stream_with_separator(syntax_str_with_separator);
  syntax = CSSSyntaxDefinition::Consume(stream_with_separator);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(stream_with_separator.Offset(), valid_syntax_str.length() + 1);

  String syntax_str = valid_syntax_str + " " + invalid_syntax_str;
  CSSParserTokenStream stream(syntax_str);
  syntax = CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(stream.Offset(), valid_syntax_str.length() + 1);
}

class SyntaxStreamOffsetTest
    : public CSSSyntaxDefinitionTest,
      public testing::WithParamInterface<std::tuple<const char*, const char*>> {
};

INSTANTIATE_TEST_SUITE_P(
    CSSSyntaxDefinitionTest,
    SyntaxStreamOffsetTest,
    testing::Combine(testing::ValuesIn(kValidSyntaxStr),
                     testing::ValuesIn(kInvalidSyntaxStr)));

TEST_P(SyntaxStreamOffsetTest, TestStreamOffsetAfterConsuming) {
  auto [valid_syntax, invalid_syntax] = GetParam();
  String valid_syntax_str(valid_syntax);
  String invalid_syntax_str(invalid_syntax);
  CSSParserTokenStream valid_syntax_stream(valid_syntax_str);
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(valid_syntax_stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(valid_syntax_stream.Offset(), valid_syntax_str.length());

  CSSParserTokenStream invalid_syntax_stream(invalid_syntax_str);
  syntax = CSSSyntaxDefinition::Consume(invalid_syntax_stream);
  ASSERT_FALSE(syntax.has_value());
  EXPECT_EQ(invalid_syntax_stream.Offset(), 0u);

  String syntax_str_with_separator =
      valid_syntax_str + " | " + invalid_syntax_str;
  CSSParserTokenStream stream_with_separator(syntax_str_with_separator);
  syntax = CSSSyntaxDefinition::Consume(stream_with_separator);
  ASSERT_FALSE(syntax.has_value());
  EXPECT_EQ(stream_with_separator.Offset(), 0u);

  String syntax_str = valid_syntax_str + " " + invalid_syntax_str;
  CSSParserTokenStream stream(syntax_str);
  syntax = CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(stream.Offset(), valid_syntax_str.length() + 1);
}

TEST_F(CSSSyntaxDefinitionTest, ConsumeSingleType) {
  CSSParserTokenStream stream("<length>");
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  EXPECT_TRUE(syntax.has_value());

  Vector<CSSSyntaxComponent> components = syntax->Components();
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0], CSSSyntaxComponent(CSSSyntaxType::kLength, String(),
                                              CSSSyntaxRepeat::kNone));
}

TEST_F(CSSSyntaxDefinitionTest, ConsumeSingleTypeWithPlusMultiplier) {
  CSSParserTokenStream stream("<number>+");
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());

  Vector<CSSSyntaxComponent> components = syntax->Components();
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0],
            CSSSyntaxComponent(CSSSyntaxType::kNumber, String(),
                               CSSSyntaxRepeat::kSpaceSeparated));
}

TEST_F(CSSSyntaxDefinitionTest, ConsumeSingleTypeWithHashMultiplier) {
  CSSParserTokenStream stream("<angle>#");
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());

  Vector<CSSSyntaxComponent> components = syntax->Components();
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0],
            CSSSyntaxComponent(CSSSyntaxType::kAngle, String(),
                               CSSSyntaxRepeat::kCommaSeparated));
}

TEST_F(CSSSyntaxDefinitionTest, ConsumeIdentType) {
  CSSParserTokenStream stream("ident+");
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());

  Vector<CSSSyntaxComponent> components = syntax->Components();
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0],
            CSSSyntaxComponent(CSSSyntaxType::kIdent, String("ident"),
                               CSSSyntaxRepeat::kSpaceSeparated));
}

TEST_F(CSSSyntaxDefinitionTest, ConsumeMultipleTypes) {
  CSSParserTokenStream stream("ident# | <url> | <length>+");
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::Consume(stream);
  ASSERT_TRUE(syntax.has_value());

  Vector<CSSSyntaxComponent> components = syntax->Components();
  ASSERT_EQ(components.size(), 3u);
  EXPECT_EQ(components[0],
            CSSSyntaxComponent(CSSSyntaxType::kIdent, String("ident"),
                               CSSSyntaxRepeat::kCommaSeparated));
  EXPECT_EQ(components[1], CSSSyntaxComponent(CSSSyntaxType::kUrl, String(),
                                              CSSSyntaxRepeat::kNone));
  EXPECT_EQ(components[2],
            CSSSyntaxComponent(CSSSyntaxType::kLength, String(),
                               CSSSyntaxRepeat::kSpaceSeparated));
}

class SyntaxStreamAndSyntaxStringComparisonTest
    : public CSSSyntaxDefinitionTest,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTestValid,
                         SyntaxStreamAndSyntaxStringComparisonTest,
                         testing::ValuesIn(kValidSyntaxStr));

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTestInvalid,
                         SyntaxStreamAndSyntaxStringComparisonTest,
                         testing::ValuesIn(kInvalidSyntaxStr));

TEST_P(SyntaxStreamAndSyntaxStringComparisonTest, TestEquality) {
  String str(GetParam());
  CSSParserTokenStream stream(str);
  std::optional<CSSSyntaxDefinition> string_syntax =
      CSSSyntaxStringParser(str).Parse();
  std::optional<CSSSyntaxDefinition> stream_syntax =
      CSSSyntaxDefinition::Consume(stream);
  EXPECT_EQ(stream_syntax, string_syntax);
}

namespace {

constexpr const char* kValidComponentData[] = {
    // clang-format off
    "auto",
    "<angle>",
    "<color>",
    "<custom-ident>",
    "<image>",
    "<integer>",
    "<length>",
    "<length-percentage>",
    "<number>",
    "<percentage>",
    "<resolution>",
    "<string>",
    "<time>",
    "<url>",
    "<transform-function>",
    "<transform-list>",
    "<angle>+",
    "<angle>#",
    "<length>#",
    "<length>#",
    // clang-format on
};

}  // namespace

class ValidComponentTest : public CSSSyntaxDefinitionTest,
                           public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTest,
                         ValidComponentTest,
                         testing::ValuesIn(kValidComponentData));

TEST_P(ValidComponentTest, All) {
  String str(GetParam());
  CSSParserTokenStream stream(str);
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::ConsumeComponent(stream);
  ASSERT_TRUE(syntax.has_value());
  EXPECT_EQ(1u, syntax->Components().size());
  EXPECT_TRUE(stream.AtEnd());
}

namespace {

constexpr const char* kInvalidComponentData[] = {
    // clang-format off
    "*",
    "<angle>++",
    "<angle>##",
    "<angle> | <color>",
    "<length> | auto",
    "auto | length",
    // <transform-list> is special: it's already a list, and can't be followed
    // by a <syntax-multiplier>.
    "<transform-list>#",
    "<transform-list>+",
    // clang-format on
};

}  // namespace

class InvalidComponentTest : public CSSSyntaxDefinitionTest,
                             public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSSyntaxDefinitionTest,
                         InvalidComponentTest,
                         testing::ValuesIn(kInvalidComponentData));

TEST_P(InvalidComponentTest, All) {
  String str(GetParam());
  CSSParserTokenStream stream(str);
  std::optional<CSSSyntaxDefinition> syntax =
      CSSSyntaxDefinition::ConsumeComponent(stream);
  // There are two acceptable failure modes: either not returning
  // a value, or not leaving the stream at the end.
  EXPECT_FALSE(syntax.has_value() && stream.AtEnd());
}

}  // namespace blink
