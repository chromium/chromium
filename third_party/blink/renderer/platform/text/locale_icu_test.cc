/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/locale_icu.h"

#include <unicode/uvernum.h>
#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class LocaleICUTest : public testing::Test {
 public:
  // Labels class is used for printing results in EXPECT_EQ macro.
  class Labels {
   public:
    Labels(const Vector<String> labels) : labels_(labels) {}

    // FIXME: We should use Vector<T>::operator==() if it works.
    bool operator==(const Labels& other) const {
      if (labels_.size() != other.labels_.size())
        return false;
      for (unsigned index = 0; index < labels_.size(); ++index) {
        if (labels_[index] != other.labels_[index])
          return false;
      }
      return true;
    }

    String ToString() const {
      StringBuilder builder;
      builder.Append("labels(");
      for (unsigned index = 0; index < labels_.size(); ++index) {
        if (index)
          builder.Append(", ");
        builder.Append('"');
        builder.Append(labels_[index]);
        builder.Append('"');
      }
      builder.Append(')');
      return builder.ToString();
    }

   private:
    Vector<String> labels_;
  };

 protected:
  Labels LabelsFromTwoElements(const String& element1, const String& element2) {
    Vector<String> labels = Vector<String>();
    labels.push_back(element1);
    labels.push_back(element2);
    return Labels(labels);
  }

  String MonthFormat(const char* locale_string) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->MonthFormat();
  }

  String LocalizedDateFormatText(const char* locale_string) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->TimeFormat();
  }

  String LocalizedShortDateFormatText(const char* locale_string) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->ShortTimeFormat();
  }

  String ShortMonthLabel(const char* locale_string, unsigned index) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->ShortMonthLabels()[index];
  }

  String ShortStandAloneMonthLabel(const char* locale_string, unsigned index) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->ShortStandAloneMonthLabels()[index];
  }

  String StandAloneMonthLabel(const char* locale_string, unsigned index) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->StandAloneMonthLabels()[index];
  }

  Labels TimeAMPMLabels(const char* locale_string) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return Labels(locale->TimeAMPMLabels());
  }

  bool IsRTL(const char* locale_string) {
    auto locale = std::make_unique<LocaleICU>(locale_string);
    return locale->IsRTL();
  }
};

std::ostream& operator<<(std::ostream& os,
                         const LocaleICUTest::Labels& labels) {
  return os << labels.ToString();
}

TEST_F(LocaleICUTest, isRTL) {
  EXPECT_TRUE(IsRTL("ar-EG"));
  EXPECT_FALSE(IsRTL("en-us"));
  EXPECT_FALSE(IsRTL("ja-jp"));
  EXPECT_FALSE(IsRTL("**invalid**"));
}

TEST_F(LocaleICUTest, monthFormat) {
  EXPECT_EQ("MMMM yyyy", MonthFormat("en_US"));
  EXPECT_EQ("MMMM yyyy", MonthFormat("fr"));
  EXPECT_EQ("yyyy\xE5\xB9\xB4M\xE6\x9C\x88", MonthFormat("ja").Utf8());
}

TEST_F(LocaleICUTest, localizedDateFormatText) {
  // Note: EXPECT_EQ(String, String) doesn't print result as string.
  EXPECT_EQ(
      "h:mm:ss\xE2\x80\xAF"
      "a",
      LocalizedDateFormatText("en_US").Utf8());
  EXPECT_EQ("HH:mm:ss", LocalizedDateFormatText("fr"));
  EXPECT_EQ("H:mm:ss", LocalizedDateFormatText("ja"));
}

TEST_F(LocaleICUTest, localizedShortDateFormatText) {
  EXPECT_EQ(
      "h:mm\xE2\x80\xAF"
      "a",
      LocalizedShortDateFormatText("en_US").Utf8());
  EXPECT_EQ("HH:mm", LocalizedShortDateFormatText("fr"));
  EXPECT_EQ("H:mm", LocalizedShortDateFormatText("ja"));
}

TEST_F(LocaleICUTest, standAloneMonthLabels) {
  EXPECT_EQ("January", StandAloneMonthLabel("en_US", 0));
  EXPECT_EQ("June", StandAloneMonthLabel("en_US", 5));
  EXPECT_EQ("December", StandAloneMonthLabel("en_US", 11));

#if U_ICU_VERSION_MAJOR_NUM >= 54
  EXPECT_EQ("Janvier", StandAloneMonthLabel("fr_FR", 0));
  EXPECT_EQ("Juin", StandAloneMonthLabel("fr_FR", 5));
  EXPECT_EQ(
      "D\xC3\xA9"
      "cembre",
      StandAloneMonthLabel("fr_FR", 11).Utf8());
#else
  EXPECT_EQ("janvier", standAloneMonthLabel("fr_FR", 0));
  EXPECT_EQ("juin", standAloneMonthLabel("fr_FR", 5));
  EXPECT_EQ(
      "d\xC3\xA9"
      "cembre",
      standAloneMonthLabel("fr_FR", 11));
#endif

  EXPECT_EQ("1\xE6\x9C\x88", StandAloneMonthLabel("ja_JP", 0).Utf8());
  EXPECT_EQ("6\xE6\x9C\x88", StandAloneMonthLabel("ja_JP", 5).Utf8());
  EXPECT_EQ("12\xE6\x9C\x88", StandAloneMonthLabel("ja_JP", 11).Utf8());

  EXPECT_EQ("\xD0\x9C\xD0\xB0\xD1\x80\xD1\x82",
            StandAloneMonthLabel("ru_RU", 2).Utf8());
  EXPECT_EQ("\xD0\x9C\xD0\xB0\xD0\xB9",
            StandAloneMonthLabel("ru_RU", 4).Utf8());
}

TEST_F(LocaleICUTest, shortMonthLabels) {
  EXPECT_EQ("Jan", ShortMonthLabel("en_US", 0));
  EXPECT_EQ("Jan", ShortStandAloneMonthLabel("en_US", 0));
  EXPECT_EQ("Dec", ShortMonthLabel("en_US", 11));
  EXPECT_EQ("Dec", ShortStandAloneMonthLabel("en_US", 11));

#if U_ICU_VERSION_MAJOR_NUM >= 54
  EXPECT_EQ("janv.", ShortMonthLabel("fr_FR", 0));
  EXPECT_EQ("Janv.", ShortStandAloneMonthLabel("fr_FR", 0));
  EXPECT_EQ(
      "d\xC3\xA9"
      "c.",
      ShortMonthLabel("fr_FR", 11).Utf8());
  EXPECT_EQ(
      "D\xC3\xA9"
      "c.",
      ShortStandAloneMonthLabel("fr_FR", 11).Utf8());
#else
  EXPECT_EQ("janv.", shortMonthLabel("fr_FR", 0));
  EXPECT_EQ("janv.", shortStandAloneMonthLabel("fr_FR", 0));
  EXPECT_EQ(
      "d\xC3\xA9"
      "c.",
      shortMonthLabel("fr_FR", 11));
  EXPECT_EQ(
      "d\xC3\xA9"
      "c.",
      shortStandAloneMonthLabel("fr_FR", 11));
#endif

  EXPECT_EQ("1\xE6\x9C\x88", ShortMonthLabel("ja_JP", 0).Utf8());
  EXPECT_EQ("1\xE6\x9C\x88", ShortStandAloneMonthLabel("ja_JP", 0).Utf8());
  EXPECT_EQ("12\xE6\x9C\x88", ShortMonthLabel("ja_JP", 11).Utf8());
  EXPECT_EQ("12\xE6\x9C\x88", ShortStandAloneMonthLabel("ja_JP", 11).Utf8());

  EXPECT_EQ("\xD0\xBC\xD0\xB0\xD1\x80.", ShortMonthLabel("ru_RU", 2).Utf8());
  EXPECT_EQ("\xD0\x9C\xD0\xB0\xD1\x80\xD1\x82",
            ShortStandAloneMonthLabel("ru_RU", 2).Utf8());
  EXPECT_EQ("\xD0\xBC\xD0\xB0\xD1\x8F", ShortMonthLabel("ru_RU", 4).Utf8());
  EXPECT_EQ("\xD0\x9C\xD0\xB0\xD0\xB9",
            ShortStandAloneMonthLabel("ru_RU", 4).Utf8());
}

TEST_F(LocaleICUTest, timeAMPMLabels) {
  EXPECT_EQ(LabelsFromTwoElements("AM", "PM"), TimeAMPMLabels("en_US"));
  EXPECT_EQ(LabelsFromTwoElements("AM", "PM"), TimeAMPMLabels("fr"));

  UChar ja_am[3] = {0x5348, 0x524d, 0};
  UChar ja_pm[3] = {0x5348, 0x5F8C, 0};
  EXPECT_EQ(LabelsFromTwoElements(String(ja_am), String(ja_pm)),
            TimeAMPMLabels("ja"));
}

static String TestDecimalSeparator(const AtomicString& locale_identifier) {
  std::unique_ptr<Locale> locale = Locale::Create(locale_identifier);
  return locale->LocalizedDecimalSeparator();
}

TEST_F(LocaleICUTest, localizedDecimalSeparator) {
  EXPECT_EQ(String("."), TestDecimalSeparator(AtomicString("en_US")));
  EXPECT_EQ(String(","), TestDecimalSeparator(AtomicString("fr")));
}

void TestNumberIsReversible(const AtomicString& locale_identifier,
                            const char* original,
                            const char* should_have = nullptr) {
  std::unique_ptr<Locale> locale = Locale::Create(locale_identifier);
  String localized = locale->ConvertToLocalizedNumber(original);
  if (should_have)
    EXPECT_TRUE(localized.Contains(should_have));
  String converted = locale->ConvertFromLocalizedNumber(localized);
  EXPECT_EQ(original, converted);
}

void TestNumbers(const char* locale) {
  AtomicString locale_string(locale);
  TestNumberIsReversible(locale_string, "123456789012345678901234567890");
  TestNumberIsReversible(locale_string, "-123.456");
  TestNumberIsReversible(locale_string, ".456");
  TestNumberIsReversible(locale_string, "-0.456");
}

TEST_F(LocaleICUTest, reversible) {
  AtomicString en_us_locale("en_US");
  TestNumberIsReversible(en_us_locale, "123456789012345678901234567890");
  TestNumberIsReversible(en_us_locale, "-123.456", ".");
  TestNumberIsReversible(en_us_locale, ".456", ".");
  TestNumberIsReversible(en_us_locale, "-0.456", ".");

  AtomicString fr_locale("fr");
  TestNumberIsReversible(fr_locale, "123456789012345678901234567890");
  TestNumberIsReversible(fr_locale, "-123.456", ",");
  TestNumberIsReversible(fr_locale, ".456", ",");
  TestNumberIsReversible(fr_locale, "-0.456", ",");

  // Persian locale has a negative prefix and a negative suffix.
  TestNumbers("fa");

  // Test some of major locales.
  TestNumbers("ar");
  TestNumbers("de_DE");
  TestNumbers("es_ES");
  TestNumbers("ja_JP");
  TestNumbers("ko_KR");
  TestNumbers("zh_CN");
  TestNumbers("zh_HK");
  TestNumbers("zh_TW");
}

}  // namespace blink
