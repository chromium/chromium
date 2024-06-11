// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"

namespace blink {

class CSSSyntaxStringParserTest : public testing::Test {
 public:
  std::optional<CSSSyntaxComponent> ParseSingleComponent(const String& syntax) {
    auto definition = CSSSyntaxStringParser(syntax).Parse();
    if (!definition) {
      return std::nullopt;
    }
    if (definition->Components().size() != 1) {
      return std::nullopt;
    }
    return definition->Components()[0];
  }

  std::optional<CSSSyntaxType> ParseSingleType(const String& syntax) {
    auto component = ParseSingleComponent(syntax);
    return component ? std::make_optional(component->GetType()) : std::nullopt;
  }

  String ParseSingleIdent(const String& syntax) {
    auto component = ParseSingleComponent(syntax);
    if (!component || component->GetType() != CSSSyntaxType::kIdent) {
      return g_empty_string;
    }
    return component->GetString();
  }

  size_t ParseNumberOfComponents(const String& syntax) {
    auto definition = CSSSyntaxStringParser(syntax).Parse();
    if (!definition) {
      return 0;
    }
    return definition->Components().size();
  }

  CSSSyntaxDefinition CreateUniversalDescriptor() {
    return CSSSyntaxDefinition::CreateUniversal();
  }
};

TEST_F(CSSSyntaxStringParserTest, UniversalDescriptor) {
  auto universal = CreateUniversalDescriptor();
  EXPECT_TRUE(universal.IsUniversal());
  EXPECT_EQ(universal, *CSSSyntaxStringParser("*").Parse());
  EXPECT_EQ(universal, *CSSSyntaxStringParser(" * ").Parse());
  EXPECT_EQ(universal, *CSSSyntaxStringParser("\r*\r\n").Parse());
  EXPECT_EQ(universal, *CSSSyntaxStringParser("\f*\f").Parse());
  EXPECT_EQ(universal, *CSSSyntaxStringParser(" \n\t\r\f*").Parse());
}

TEST_F(CSSSyntaxStringParserTest, ValidDataType) {
  EXPECT_EQ(CSSSyntaxType::kLength, *ParseSingleType("<length>"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("<number>"));
  EXPECT_EQ(CSSSyntaxType::kPercentage, *ParseSingleType("<percentage>"));
  EXPECT_EQ(CSSSyntaxType::kLengthPercentage,
            *ParseSingleType("<length-percentage>"));
  EXPECT_EQ(CSSSyntaxType::kColor, *ParseSingleType("<color>"));
  EXPECT_EQ(CSSSyntaxType::kImage, *ParseSingleType("<image>"));
  EXPECT_EQ(CSSSyntaxType::kUrl, *ParseSingleType("<url>"));
  EXPECT_EQ(CSSSyntaxType::kInteger, *ParseSingleType("<integer>"));
  EXPECT_EQ(CSSSyntaxType::kAngle, *ParseSingleType("<angle>"));
  EXPECT_EQ(CSSSyntaxType::kTime, *ParseSingleType("<time>"));
  EXPECT_EQ(CSSSyntaxType::kResolution, *ParseSingleType("<resolution>"));
  EXPECT_EQ(CSSSyntaxType::kTransformFunction,
            *ParseSingleType("<transform-function>"));
  EXPECT_EQ(CSSSyntaxType::kTransformList,
            *ParseSingleType("<transform-list>"));
  EXPECT_EQ(CSSSyntaxType::kCustomIdent, *ParseSingleType("<custom-ident>"));

  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType(" <number>"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("\r\n<number>"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("  \t <number>"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("<number> "));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("<number>\n"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("<number>\r\n"));
  EXPECT_EQ(CSSSyntaxType::kNumber, *ParseSingleType("\f<number>\f"));
}

TEST_F(CSSSyntaxStringParserTest, InvalidDataType) {
  EXPECT_FALSE(CSSSyntaxStringParser("< length>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<length >").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<\tlength >").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser(">").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("< >").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<length").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<\\61>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser(" <\\61> ").Parse());

  // Syntactically valid, but names unsupported data types.
  EXPECT_FALSE(CSSSyntaxStringParser("<unsupported>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<lengths>").Parse());
}

TEST_F(CSSSyntaxStringParserTest, Idents) {
  EXPECT_EQ("foo", ParseSingleIdent("foo"));
  EXPECT_EQ("foo", ParseSingleIdent(" foo"));
  EXPECT_EQ("foo", ParseSingleIdent("foo "));
  EXPECT_EQ("foo", ParseSingleIdent("foo "));
  EXPECT_EQ("foo", ParseSingleIdent("\t\rfoo "));
  EXPECT_EQ("_foo", ParseSingleIdent("_foo "));
  EXPECT_EQ("foo-bar", ParseSingleIdent("foo-bar"));
  EXPECT_EQ("abc", ParseSingleIdent("\\61 b\\63"));
  EXPECT_EQ("azc", ParseSingleIdent("\\61z\\63"));
}

TEST_F(CSSSyntaxStringParserTest, InvalidIdents) {
  EXPECT_FALSE(CSSSyntaxStringParser("-foo").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("007").Parse());

  EXPECT_FALSE(CSSSyntaxStringParser("initial").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("inherit").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("unset").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("default").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("revert").Parse());
}

TEST_F(CSSSyntaxStringParserTest, Combinator) {
  {
    auto desc = CSSSyntaxStringParser("<length> | <color>").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(2u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kLength, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxType::kColor, desc->Components()[1].GetType());
  }

  {
    auto desc = CSSSyntaxStringParser("<integer> | foo | <color>").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(3u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kInteger, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[1].GetType());
    EXPECT_EQ(CSSSyntaxType::kColor, desc->Components()[2].GetType());

    EXPECT_EQ("foo", desc->Components()[1].GetString());
  }

  {
    auto desc = CSSSyntaxStringParser("a|\\62|c").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(3u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[1].GetType());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[2].GetType());
    EXPECT_EQ("a", desc->Components()[0].GetString());
    EXPECT_EQ("b", desc->Components()[1].GetString());
    EXPECT_EQ("c", desc->Components()[2].GetString());
  }
}

TEST_F(CSSSyntaxStringParserTest, CombinatorWhitespace) {
  EXPECT_EQ(2u, ParseNumberOfComponents("<length>|<color>"));
  EXPECT_EQ(3u, ParseNumberOfComponents("a|<color>|b"));
  EXPECT_EQ(3u, ParseNumberOfComponents("a\t\n|  <color>\r\n  |  b "));
}

TEST_F(CSSSyntaxStringParserTest, InvalidCombinator) {
  EXPECT_FALSE(CSSSyntaxStringParser("|<color>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("\f|  <color>").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("a||b").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("a|  |b").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("a|\t|b").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("|").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("foo|").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("foo||").Parse());
}

TEST_F(CSSSyntaxStringParserTest, Multipliers) {
  {
    auto desc = CSSSyntaxStringParser("<length>").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(1u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kLength, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kNone, desc->Components()[0].GetRepeat());
  }

  {
    auto desc = CSSSyntaxStringParser("foo").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(1u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kNone, desc->Components()[0].GetRepeat());
  }

  {
    auto desc = CSSSyntaxStringParser("<length>+").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(1u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kLength, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kSpaceSeparated,
              desc->Components()[0].GetRepeat());
  }

  {
    auto desc = CSSSyntaxStringParser("<color>#").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(1u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kColor, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kCommaSeparated,
              desc->Components()[0].GetRepeat());
  }

  {
    auto desc = CSSSyntaxStringParser("foo#").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(1u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kCommaSeparated,
              desc->Components()[0].GetRepeat());
  }
}

TEST_F(CSSSyntaxStringParserTest, InvalidMultipliers) {
  EXPECT_FALSE(CSSSyntaxStringParser("<length>*").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<length>?").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<length> +").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<color>\t#").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("foo #").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("foo{4}").Parse());

  // Stacking multipliers may supported in the future, but it's currently
  // not allowed by the spec.
  EXPECT_FALSE(CSSSyntaxStringParser("<length>+#").Parse());
}

TEST_F(CSSSyntaxStringParserTest, CombinatorWithMultipliers) {
  {
    auto desc = CSSSyntaxStringParser("<length>+ | <color>#").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(2u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kLength, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kSpaceSeparated,
              desc->Components()[0].GetRepeat());
    EXPECT_EQ(CSSSyntaxType::kColor, desc->Components()[1].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kCommaSeparated,
              desc->Components()[1].GetRepeat());
  }

  {
    auto desc = CSSSyntaxStringParser("<length>+ | <color> | foo#").Parse();
    ASSERT_TRUE(desc);
    EXPECT_EQ(3u, desc->Components().size());
    EXPECT_EQ(CSSSyntaxType::kLength, desc->Components()[0].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kSpaceSeparated,
              desc->Components()[0].GetRepeat());
    EXPECT_EQ(CSSSyntaxType::kColor, desc->Components()[1].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kNone, desc->Components()[1].GetRepeat());
    EXPECT_EQ(CSSSyntaxType::kIdent, desc->Components()[2].GetType());
    EXPECT_EQ(CSSSyntaxRepeat::kCommaSeparated,
              desc->Components()[2].GetRepeat());
  }
}

TEST_F(CSSSyntaxStringParserTest, PreMultiplied) {
  // Multipliers may not be used on data type names that are pre-multiplied.
  EXPECT_FALSE(CSSSyntaxStringParser("<transform-list>#").Parse());
  EXPECT_FALSE(CSSSyntaxStringParser("<transform-list>+").Parse());
}

}  // namespace blink
