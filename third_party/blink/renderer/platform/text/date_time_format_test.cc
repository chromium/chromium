/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/date_time_format.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class DateTimeFormatTest : public testing::Test {
 public:
  using FieldType = DateTimeFormat::FieldType;

  struct Token {
    String string;
    int count;
    FieldType field_type;

    Token(FieldType field_type, int count = 1)
        : count(count), field_type(field_type) {
      DCHECK_NE(field_type, DateTimeFormat::kFieldTypeLiteral);
    }

    Token(const String& string)
        : string(string),
          count(0),
          field_type(DateTimeFormat::kFieldTypeLiteral) {}

    bool operator==(const Token& other) const {
      return field_type == other.field_type && count == other.count &&
             string == other.string;
    }

    String ToString() const {
      switch (field_type) {
        case DateTimeFormat::kFieldTypeInvalid:
          return "*invalid*";
        case DateTimeFormat::kFieldTypeLiteral: {
          StringBuilder builder;
          builder.Append('"');
          builder.Append(string);
          builder.Append('"');
          return builder.ToString();
        }
        default:
          return String::Format("Token(%d, %d)", field_type, count);
      }
    }
  };

  class Tokens {
   public:
    Tokens() = default;

    explicit Tokens(const Vector<Token> tokens) : tokens_(tokens) {}

    explicit Tokens(const String& string) { tokens_.push_back(Token(string)); }

    explicit Tokens(Token token1) { tokens_.push_back(token1); }

    Tokens(Token token1, Token token2) {
      tokens_.push_back(token1);
      tokens_.push_back(token2);
    }

    Tokens(Token token1, Token token2, Token token3) {
      tokens_.push_back(token1);
      tokens_.push_back(token2);
      tokens_.push_back(token3);
    }

    Tokens(Token token1, Token token2, Token token3, Token token4) {
      tokens_.push_back(token1);
      tokens_.push_back(token2);
      tokens_.push_back(token3);
      tokens_.push_back(token4);
    }

    Tokens(Token token1,
           Token token2,
           Token token3,
           Token token4,
           Token token5) {
      tokens_.push_back(token1);
      tokens_.push_back(token2);
      tokens_.push_back(token3);
      tokens_.push_back(token4);
      tokens_.push_back(token5);
    }

    Tokens(Token token1,
           Token token2,
           Token token3,
           Token token4,
           Token token5,
           Token token6) {
      tokens_.push_back(token1);
      tokens_.push_back(token2);
      tokens_.push_back(token3);
      tokens_.push_back(token4);
      tokens_.push_back(token5);
      tokens_.push_back(token6);
    }

    bool operator==(const Tokens& other) const {
      return tokens_ == other.tokens_;
    }

    String ToString() const {
      StringBuilder builder;
      builder.Append("Tokens(");
      for (unsigned index = 0; index < tokens_.size(); ++index) {
        if (index)
          builder.Append(',');
        builder.Append(tokens_[index].ToString());
      }
      builder.Append(')');
      return builder.ToString();
    }

   private:
    Vector<Token> tokens_;
  };

 protected:
  Tokens Parse(const String& format_string) {
    TokenHandler handler;
    if (!DateTimeFormat::Parse(format_string, handler))
      return Tokens(Token("*failed*"));
    return handler.GetTokens();
  }

  FieldType Single(const char ch) {
    char format_string[2];
    format_string[0] = ch;
    format_string[1] = 0;
    TokenHandler handler;
    if (!DateTimeFormat::Parse(format_string, handler))
      return DateTimeFormat::kFieldTypeInvalid;
    return handler.GetFieldType(0);
  }

 private:
  class TokenHandler : public DateTimeFormat::TokenHandler {
   public:
    ~TokenHandler() override = default;

    FieldType GetFieldType(int index) const {
      return index >= 0 && index < static_cast<int>(tokens_.size())
                 ? tokens_[index].field_type
                 : DateTimeFormat::kFieldTypeInvalid;
    }

    Tokens GetTokens() const { return Tokens(tokens_); }

   private:
    void VisitField(FieldType field_type, int count) override {
      tokens_.push_back(Token(field_type, count));
    }

    void VisitLiteral(const String& string) override {
      tokens_.push_back(Token(string));
    }

    Vector<Token> tokens_;
  };
};

std::ostream& operator<<(std::ostream& os,
                         const DateTimeFormatTest::Tokens& tokens) {
  return os << tokens.ToString();
}

TEST_F(DateTimeFormatTest, CommonPattern) {
  EXPECT_EQ(Tokens(), Parse(""));

  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeYear, 4), Token("-"),
                   Token(DateTimeFormat::kFieldTypeMonth, 2), Token("-"),
                   Token(DateTimeFormat::kFieldTypeDayOfMonth, 2)),
            Parse("yyyy-MM-dd"));

  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeHour24, 2), Token(":"),
                   Token(DateTimeFormat::kFieldTypeMinute, 2), Token(":"),
                   Token(DateTimeFormat::kFieldTypeSecond, 2)),
            Parse("kk:mm:ss"));

  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeHour12), Token(":"),
                   Token(DateTimeFormat::kFieldTypeMinute), Token(" "),
                   Token(DateTimeFormat::kFieldTypePeriod)),
            Parse("h:m a"));

  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeYear), Token("Nen "),
                   Token(DateTimeFormat::kFieldTypeMonth), Token("Getsu "),
                   Token(DateTimeFormat::kFieldTypeDayOfMonth), Token("Nichi")),
            Parse("y'Nen' M'Getsu' d'Nichi'"));
}

TEST_F(DateTimeFormatTest, MissingClosingQuote) {
  EXPECT_EQ(Tokens("*failed*"), Parse("'foo"));
  EXPECT_EQ(Tokens("*failed*"), Parse("fo'o"));
  EXPECT_EQ(Tokens("*failed*"), Parse("foo'"));
}

TEST_F(DateTimeFormatTest, Quote) {
  EXPECT_EQ(Tokens("FooBar"), Parse("'FooBar'"));
  EXPECT_EQ(Tokens("'"), Parse("''"));
  EXPECT_EQ(Tokens("'-'"), Parse("''-''"));
  EXPECT_EQ(Tokens("Foo'Bar"), Parse("'Foo''Bar'"));
  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeEra), Token("'s")),
            Parse("G'''s'"));
  EXPECT_EQ(Tokens(Token(DateTimeFormat::kFieldTypeEra), Token("'"),
                   Token(DateTimeFormat::kFieldTypeSecond)),
            Parse("G''s"));
}

TEST_F(DateTimeFormatTest, SingleLowerCaseCharacter) {
  EXPECT_EQ(DateTimeFormat::kFieldTypePeriodAmPmNoonMidnight, Single('b'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeLocalDayOfWeekStandAlon, Single('c'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeDayOfMonth, Single('d'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeLocalDayOfWeek, Single('e'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeModifiedJulianDay, Single('g'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeHour12, Single('h'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeHour24, Single('k'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeMinute, Single('m'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeQuaterStandAlone, Single('q'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeYearRelatedGregorian, Single('r'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeSecond, Single('s'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeExtendedYear, Single('u'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeNonLocationZone, Single('v'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeWeekOfMonth, Single('W'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeZoneIso8601, Single('x'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeYear, Single('y'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeZone, Single('z'));
}

TEST_F(DateTimeFormatTest, SingleLowerCaseInvalid) {
  EXPECT_EQ(DateTimeFormat::kFieldTypePeriod, Single('a'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('f'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('i'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('j'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('l'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('n'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('o'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('p'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('t'));
}

TEST_F(DateTimeFormatTest, SingleUpperCaseCharacter) {
  EXPECT_EQ(DateTimeFormat::kFieldTypeMillisecondsInDay, Single('A'));
  EXPECT_EQ(DateTimeFormat::kFieldTypePeriodFlexible, Single('B'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeDayOfYear, Single('D'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeDayOfWeek, Single('E'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeDayOfWeekInMonth, Single('F'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeEra, Single('G'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeHour23, Single('H'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeHour11, Single('K'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeMonthStandAlone, Single('L'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeMonth, Single('M'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeZoneLocalized, Single('O'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeQuater, Single('Q'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeFractionalSecond, Single('S'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeYearCyclicName, Single('U'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeZoneId, Single('V'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeWeekOfYear, Single('w'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeZoneIso8601Z, Single('X'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeYearOfWeekOfYear, Single('Y'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeRFC822Zone, Single('Z'));
}

TEST_F(DateTimeFormatTest, SingleUpperCaseInvalid) {
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('C'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('I'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('J'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('N'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('P'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('R'));
  EXPECT_EQ(DateTimeFormat::kFieldTypeInvalid, Single('T'));
}

}  // namespace blink
