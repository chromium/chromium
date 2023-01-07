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

#include "third_party/blink/renderer/platform/text/locale_win.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class LocaleWinTest : public testing::Test {
 protected:
  enum {
    kJanuary = 0,
    kFebruary,
    kMarch,
    kApril,
    kMay,
    kJune,
    kJuly,
    kAugust,
    kSeptember,
    kOctober,
    kNovember,
    kDecember,
  };

  enum {
    kSunday = 0,
    kMonday,
    kTuesday,
    kWednesday,
    kThursday,
    kFriday,
    kSaturday,
  };

  // See http://msdn.microsoft.com/en-us/goglobal/bb964664.aspx
  // Note that some locales are country-neutral.
  enum {
    kArabicEG = 0x0C01,   // ar-eg
    kChineseCN = 0x0804,  // zh-cn
    kChineseHK = 0x0C04,  // zh-hk
    kChineseTW = 0x0404,  // zh-tw
    kGerman = 0x0407,     // de
    kEnglishUS = 0x409,   // en-us
    kFrenchFR = 0x40C,    // fr
    kJapaneseJP = 0x411,  // ja
    kKoreanKR = 0x0412,   // ko
    kPersian = 0x0429,    // fa
    kSpanish = 0x040A,    // es
  };

  DateComponents GetDateComponents(int year, int month, int day) {
    DateComponents date;
    date.SetMillisecondsSinceEpochForDate(MsForDate(year, month, day));
    return date;
  }

  double MsForDate(int year, int month, int day) {
    return DateToDaysFrom1970(year, month, day) * kMsPerDay;
  }

  String FormatDate(LCID lcid, int year, int month, int day) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->FormatDateTime(GetDateComponents(year, month, day));
  }

  unsigned FirstDayOfWeek(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->FirstDayOfWeek();
  }

  String MonthLabel(LCID lcid, unsigned index) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->MonthLabels()[index];
  }

  String WeekDayShortLabel(LCID lcid, unsigned index) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->WeekDayShortLabels()[index];
  }

  bool IsRTL(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->IsRTL();
  }

  String MonthFormat(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->MonthFormat();
  }

  String TimeFormat(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->TimeFormat();
  }

  String ShortTimeFormat(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->ShortTimeFormat();
  }

  String ShortMonthLabel(LCID lcid, unsigned index) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->ShortMonthLabels()[index];
  }

  String TimeAMPMLabel(LCID lcid, unsigned index) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->TimeAMPMLabels()[index];
  }

  String DecimalSeparator(LCID lcid) {
    std::unique_ptr<LocaleWin> locale =
        LocaleWin::Create(lcid, true /* defaultsForLocale */);
    return locale->LocalizedDecimalSeparator();
  }
};

TEST_F(LocaleWinTest, formatDate) {
  EXPECT_EQ("04/27/2005", FormatDate(kEnglishUS, 2005, kApril, 27));
  EXPECT_EQ("27/04/2005", FormatDate(kFrenchFR, 2005, kApril, 27));
  EXPECT_EQ("2005/04/27", FormatDate(kJapaneseJP, 2005, kApril, 27));
}

TEST_F(LocaleWinTest, firstDayOfWeek) {
  EXPECT_EQ(static_cast<unsigned>(kSunday), FirstDayOfWeek(kEnglishUS));
  EXPECT_EQ(static_cast<unsigned>(kMonday), FirstDayOfWeek(kFrenchFR));
  EXPECT_EQ(static_cast<unsigned>(kSunday), FirstDayOfWeek(kJapaneseJP));
}

TEST_F(LocaleWinTest, monthLabels) {
  EXPECT_EQ("January", MonthLabel(kEnglishUS, kJanuary));
  EXPECT_EQ("June", MonthLabel(kEnglishUS, kJune));
  EXPECT_EQ("December", MonthLabel(kEnglishUS, kDecember));

  EXPECT_EQ("janvier", MonthLabel(kFrenchFR, kJanuary));
  EXPECT_EQ("juin", MonthLabel(kFrenchFR, kJune));
  EXPECT_EQ(
      "d\xC3\xA9"
      "cembre",
      MonthLabel(kFrenchFR, kDecember).Utf8());

  EXPECT_EQ("1\xE6\x9C\x88", MonthLabel(kJapaneseJP, kJanuary).Utf8());
  EXPECT_EQ("6\xE6\x9C\x88", MonthLabel(kJapaneseJP, kJune).Utf8());
  EXPECT_EQ("12\xE6\x9C\x88", MonthLabel(kJapaneseJP, kDecember).Utf8());
}

TEST_F(LocaleWinTest, weekDayShortLabels) {
  EXPECT_EQ("Su", WeekDayShortLabel(kEnglishUS, kSunday));
  EXPECT_EQ("We", WeekDayShortLabel(kEnglishUS, kWednesday));
  EXPECT_EQ("Sa", WeekDayShortLabel(kEnglishUS, kSaturday));

  EXPECT_EQ("di", WeekDayShortLabel(kFrenchFR, kSunday));
  EXPECT_EQ("me", WeekDayShortLabel(kFrenchFR, kWednesday));
  EXPECT_EQ("sa", WeekDayShortLabel(kFrenchFR, kSaturday));
  EXPECT_EQ("\xE6\x97\xA5", WeekDayShortLabel(kJapaneseJP, kSunday).Utf8());
  EXPECT_EQ("\xE6\xB0\xB4", WeekDayShortLabel(kJapaneseJP, kWednesday).Utf8());
  EXPECT_EQ("\xE5\x9C\x9F", WeekDayShortLabel(kJapaneseJP, kSaturday).Utf8());
}

TEST_F(LocaleWinTest, isRTL) {
  EXPECT_TRUE(IsRTL(kArabicEG));
  EXPECT_FALSE(IsRTL(kEnglishUS));
}

TEST_F(LocaleWinTest, dateFormat) {
  EXPECT_EQ("y-M-d", LocaleWin::DateFormat("y-M-d"));
  EXPECT_EQ("''yy'-'''MM'''-'dd", LocaleWin::DateFormat("''yy-''MM''-dd"));
  EXPECT_EQ("yyyy'-''''-'MMM'''''-'dd",
            LocaleWin::DateFormat("yyyy-''''-MMM''''-dd"));
  EXPECT_EQ("yyyy'-'''''MMMM-dd", LocaleWin::DateFormat("yyyy-''''MMMM-dd"));
}

TEST_F(LocaleWinTest, monthFormat) {
  // Month format for EnglishUS:
  //  "MMMM, yyyy" on Windows 7 or older.
  //  "MMMM yyyy" on Window 8 or later.
  EXPECT_EQ("MMMM yyyy", MonthFormat(kEnglishUS).Replace(',', ""));
  EXPECT_EQ("MMMM yyyy", MonthFormat(kFrenchFR));
  EXPECT_EQ("yyyy\xE5\xB9\xB4M\xE6\x9C\x88", MonthFormat(kJapaneseJP).Utf8());
}

TEST_F(LocaleWinTest, timeFormat) {
  EXPECT_EQ("h:mm:ss a", TimeFormat(kEnglishUS));
  EXPECT_EQ("HH:mm:ss", TimeFormat(kFrenchFR));
  EXPECT_EQ("H:mm:ss", TimeFormat(kJapaneseJP));
}

TEST_F(LocaleWinTest, shortTimeFormat) {
  EXPECT_EQ("h:mm a", ShortTimeFormat(kEnglishUS));
  EXPECT_EQ("HH:mm", ShortTimeFormat(kFrenchFR));
  EXPECT_EQ("H:mm", ShortTimeFormat(kJapaneseJP));
}

TEST_F(LocaleWinTest, shortMonthLabels) {
  EXPECT_EQ("Jan", ShortMonthLabel(kEnglishUS, 0));
  EXPECT_EQ("Dec", ShortMonthLabel(kEnglishUS, 11));
  EXPECT_EQ("janv.", ShortMonthLabel(kFrenchFR, 0));
  EXPECT_EQ(
      "d\xC3\xA9"
      "c.",
      ShortMonthLabel(kFrenchFR, 11).Utf8());
  EXPECT_EQ("1", ShortMonthLabel(kJapaneseJP, 0));
  EXPECT_EQ("12", ShortMonthLabel(kJapaneseJP, 11));
}

TEST_F(LocaleWinTest, timeAMPMLabels) {
  EXPECT_EQ("AM", TimeAMPMLabel(kEnglishUS, 0));
  EXPECT_EQ("PM", TimeAMPMLabel(kEnglishUS, 1));

  EXPECT_EQ("", TimeAMPMLabel(kFrenchFR, 0).Utf8());
  EXPECT_EQ("", TimeAMPMLabel(kFrenchFR, 1).Utf8());

  EXPECT_EQ("\xE5\x8D\x88\xE5\x89\x8D", TimeAMPMLabel(kJapaneseJP, 0).Utf8());
  EXPECT_EQ("\xE5\x8D\x88\xE5\xBE\x8C", TimeAMPMLabel(kJapaneseJP, 1).Utf8());
}

TEST_F(LocaleWinTest, decimalSeparator) {
  EXPECT_EQ(".", DecimalSeparator(kEnglishUS));
  EXPECT_EQ(",", DecimalSeparator(kFrenchFR));
}

static void TestNumberIsReversible(LCID lcid,
                                   const char* original,
                                   const char* should_have = 0) {
  std::unique_ptr<LocaleWin> locale =
      LocaleWin::Create(lcid, true /* defaultsForLocale */);
  String localized = locale->ConvertToLocalizedNumber(original);
  if (should_have)
    EXPECT_TRUE(localized.Contains(should_have));
  String converted = locale->ConvertFromLocalizedNumber(localized);
  EXPECT_EQ(original, converted);
}

void TestNumbers(LCID lcid) {
  TestNumberIsReversible(lcid, "123456789012345678901234567890");
  TestNumberIsReversible(lcid, "-123.456");
  TestNumberIsReversible(lcid, ".456");
  TestNumberIsReversible(lcid, "-0.456");
}

TEST_F(LocaleWinTest, localizedNumberRoundTrip) {
  TestNumberIsReversible(kEnglishUS, "123456789012345678901234567890");
  TestNumberIsReversible(kEnglishUS, "-123.456", ".");
  TestNumberIsReversible(kEnglishUS, ".456", ".");
  TestNumberIsReversible(kEnglishUS, "-0.456", ".");

  TestNumberIsReversible(kFrenchFR, "123456789012345678901234567890");
  TestNumberIsReversible(kFrenchFR, "-123.456", ",");
  TestNumberIsReversible(kFrenchFR, ".456", ",");
  TestNumberIsReversible(kFrenchFR, "-0.456", ",");

  // Test some of major locales.
  TestNumbers(kArabicEG);
  TestNumbers(kGerman);
  TestNumbers(kSpanish);
  TestNumbers(kPersian);
  TestNumbers(kJapaneseJP);
  TestNumbers(kKoreanKR);
  TestNumbers(kChineseCN);
  TestNumbers(kChineseHK);
  TestNumbers(kChineseTW);
}

}  // namespace blink
