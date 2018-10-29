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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/locale_mac.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/date_components.h"
#include "third_party/blink/renderer/platform/mac/version_util_mac.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"

namespace blink {

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(WebLocalizedString::Name /*name*/) override {
    return WebString::FromUTF8("Week $2, $1");
  }
};

class LocaleMacTest : public testing::Test {
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

  DateComponents GetDateComponents(int year, int month, int day) {
    DateComponents date;
    date.SetMillisecondsSinceEpochForDate(MsForDate(year, month, day));
    return date;
  }

  DateComponents GetTimeComponents(int hour,
                                   int minute,
                                   int second,
                                   int millisecond) {
    DateComponents date;
    date.SetMillisecondsSinceMidnight(hour * kMsPerHour +
                                      minute * kMsPerMinute +
                                      second * kMsPerSecond + millisecond);
    return date;
  }

  double MsForDate(int year, int month, int day) {
    return DateToDaysFrom1970(year, month, day) * kMsPerDay;
  }

  String FormatWeek(const String& locale_string, const String& iso_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    DateComponents date;
    unsigned end;
    date.ParseWeek(iso_string, 0, end);
    return locale->FormatDateTime(date);
  }

  String FormatMonth(const String& locale_string,
                     const String& iso_string,
                     bool use_short_format) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    DateComponents date;
    unsigned end;
    date.ParseMonth(iso_string, 0, end);
    return locale->FormatDateTime(
        date, (use_short_format ? Locale::kFormatTypeShort
                                : Locale::kFormatTypeMedium));
  }

  String FormatDate(const String& locale_string, int year, int month, int day) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->FormatDateTime(GetDateComponents(year, month, day));
  }

  String FormatTime(const String& locale_string,
                    int hour,
                    int minute,
                    int second,
                    int millisecond,
                    bool use_short_format) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->FormatDateTime(
        GetTimeComponents(hour, minute, second, millisecond),
        (use_short_format ? Locale::kFormatTypeShort
                          : Locale::kFormatTypeMedium));
  }

  unsigned FirstDayOfWeek(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->FirstDayOfWeek();
  }

  String MonthLabel(const String& locale_string, unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->MonthLabels()[index];
  }

  String WeekDayShortLabel(const String& locale_string, unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->WeekDayShortLabels()[index];
  }

  bool IsRTL(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->IsRTL();
  }

  String MonthFormat(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->MonthFormat();
  }

  String TimeFormat(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->TimeFormat();
  }

  String ShortTimeFormat(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->ShortTimeFormat();
  }

  String ShortMonthLabel(const String& locale_string, unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->ShortMonthLabels()[index];
  }

  String StandAloneMonthLabel(const String& locale_string, unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->StandAloneMonthLabels()[index];
  }

  String ShortStandAloneMonthLabel(const String& locale_string,
                                   unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->ShortStandAloneMonthLabels()[index];
  }

  String TimeAMPMLabel(const String& locale_string, unsigned index) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->TimeAMPMLabels()[index];
  }

  String DecimalSeparator(const String& locale_string) {
    std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
    return locale->LocalizedDecimalSeparator();
  }
};

TEST_F(LocaleMacTest, formatWeek) {
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;
  EXPECT_STREQ("Week 04, 2005", FormatWeek("en_US", "2005-W04").Utf8().data());
  EXPECT_STREQ("Week 52, 2005", FormatWeek("en_US", "2005-W52").Utf8().data());
}

TEST_F(LocaleMacTest, formatMonth) {
  EXPECT_STREQ("April 2005",
               FormatMonth("en_US", "2005-04", false).Utf8().data());
  EXPECT_STREQ("avril 2005",
               FormatMonth("fr_FR", "2005-04", false).Utf8().data());
  EXPECT_STREQ(
      "2005\xE5\xB9\xB4"
      "04\xE6\x9C\x88",
      FormatMonth("ja_JP", "2005-04", false).Utf8().data());

  EXPECT_STREQ("Apr 2005", FormatMonth("en_US", "2005-04", true).Utf8().data());
  EXPECT_STREQ("avr. 2005",
               FormatMonth("fr_FR", "2005-04", true).Utf8().data());
  EXPECT_STREQ(
      "2005\xE5\xB9\xB4"
      "04\xE6\x9C\x88",
      FormatMonth("ja_JP", "2005-04", true).Utf8().data());
}

TEST_F(LocaleMacTest, formatDate) {
  EXPECT_STREQ("04/27/2005",
               FormatDate("en_US", 2005, kApril, 27).Utf8().data());
  EXPECT_STREQ("27/04/2005",
               FormatDate("fr_FR", 2005, kApril, 27).Utf8().data());
  // Do not test ja_JP locale. OS X 10.8 and 10.7 have different formats.
}

TEST_F(LocaleMacTest, formatTime) {
  // On MacOS 10.13+, Arabic times (which contain spaces) use \xC2\xA0
  // (which is a non-breaking space) instead of \x20 for those spaces. The
  // 10.13+ behavior is probably more correct, but there does not appear to be a
  // way to configure NSDateFormatter to behave that way on < 10.13.
  bool expect_ar_nbsp = !IsOS10_10() && !IsOS10_11() && !IsOS10_12();

  EXPECT_STREQ("1:23 PM",
               FormatTime("en_US", 13, 23, 00, 000, true).Utf8().data());
  EXPECT_STREQ("13:23",
               FormatTime("fr_FR", 13, 23, 00, 000, true).Utf8().data());
  EXPECT_STREQ("13:23",
               FormatTime("ja_JP", 13, 23, 00, 000, true).Utf8().data());
  if (expect_ar_nbsp) {
    EXPECT_STREQ("\xD9\xA1:\xD9\xA2\xD9\xA3\xC2\xA0\xD9\x85",
                 FormatTime("ar", 13, 23, 00, 000, true).Utf8().data());
  } else {
    EXPECT_STREQ("\xD9\xA1:\xD9\xA2\xD9\xA3 \xD9\x85",
                 FormatTime("ar", 13, 23, 00, 000, true).Utf8().data());
  }
  EXPECT_STREQ("\xDB\xB1\xDB\xB3:\xDB\xB2\xDB\xB3",
               FormatTime("fa", 13, 23, 00, 000, true).Utf8().data());

  EXPECT_STREQ("12:00 AM",
               FormatTime("en_US", 00, 00, 00, 000, true).Utf8().data());
  EXPECT_STREQ("00:00",
               FormatTime("fr_FR", 00, 00, 00, 000, true).Utf8().data());
  EXPECT_STREQ("0:00",
               FormatTime("ja_JP", 00, 00, 00, 000, true).Utf8().data());
  if (expect_ar_nbsp) {
    EXPECT_STREQ("\xD9\xA1\xD9\xA2:\xD9\xA0\xD9\xA0\xC2\xA0\xD8\xB5",
                 FormatTime("ar", 00, 00, 00, 000, true).Utf8().data());
  } else {
    EXPECT_STREQ("\xD9\xA1\xD9\xA2:\xD9\xA0\xD9\xA0 \xD8\xB5",
                 FormatTime("ar", 00, 00, 00, 000, true).Utf8().data());
  }
  EXPECT_STREQ("\xDB\xB0:\xDB\xB0\xDB\xB0",
               FormatTime("fa", 00, 00, 00, 000, true).Utf8().data());

  EXPECT_STREQ("7:07:07.007 AM",
               FormatTime("en_US", 07, 07, 07, 007, false).Utf8().data());
  EXPECT_STREQ("07:07:07,007",
               FormatTime("fr_FR", 07, 07, 07, 007, false).Utf8().data());
  EXPECT_STREQ("7:07:07.007",
               FormatTime("ja_JP", 07, 07, 07, 007, false).Utf8().data());
  if (expect_ar_nbsp) {
    EXPECT_STREQ(
        "\xD9\xA7:\xD9\xA0\xD9\xA7:"
        "\xD9\xA0\xD9\xA7\xD9\xAB\xD9\xA0\xD9\xA0\xD9\xA7\xC2\xA0\xD8\xB5",
        FormatTime("ar", 07, 07, 07, 007, false).Utf8().data());
  } else {
    EXPECT_STREQ(
        "\xD9\xA7:\xD9\xA0\xD9\xA7:"
        "\xD9\xA0\xD9\xA7\xD9\xAB\xD9\xA0\xD9\xA0\xD9\xA7 \xD8\xB5",
        FormatTime("ar", 07, 07, 07, 007, false).Utf8().data());
  }
  EXPECT_STREQ(
      "\xDB\xB7:\xDB\xB0\xDB\xB7:"
      "\xDB\xB0\xDB\xB7\xD9\xAB\xDB\xB0\xDB\xB0\xDB\xB7",
      FormatTime("fa", 07, 07, 07, 007, false).Utf8().data());
}

TEST_F(LocaleMacTest, firstDayOfWeek) {
  EXPECT_EQ(kSunday, FirstDayOfWeek("en_US"));
  EXPECT_EQ(kMonday, FirstDayOfWeek("fr_FR"));
  EXPECT_EQ(kSunday, FirstDayOfWeek("ja_JP"));
}

TEST_F(LocaleMacTest, monthLabels) {
  EXPECT_STREQ("January", MonthLabel("en_US", kJanuary).Utf8().data());
  EXPECT_STREQ("June", MonthLabel("en_US", kJune).Utf8().data());
  EXPECT_STREQ("December", MonthLabel("en_US", kDecember).Utf8().data());

  EXPECT_STREQ("janvier", MonthLabel("fr_FR", kJanuary).Utf8().data());
  EXPECT_STREQ("juin", MonthLabel("fr_FR", kJune).Utf8().data());
  EXPECT_STREQ(
      "d\xC3\xA9"
      "cembre",
      MonthLabel("fr_FR", kDecember).Utf8().data());

  EXPECT_STREQ("1\xE6\x9C\x88", MonthLabel("ja_JP", kJanuary).Utf8().data());
  EXPECT_STREQ("6\xE6\x9C\x88", MonthLabel("ja_JP", kJune).Utf8().data());
  EXPECT_STREQ("12\xE6\x9C\x88", MonthLabel("ja_JP", kDecember).Utf8().data());
}

TEST_F(LocaleMacTest, weekDayShortLabels) {
  EXPECT_STREQ("Sun", WeekDayShortLabel("en_US", kSunday).Utf8().data());
  EXPECT_STREQ("Wed", WeekDayShortLabel("en_US", kWednesday).Utf8().data());
  EXPECT_STREQ("Sat", WeekDayShortLabel("en_US", kSaturday).Utf8().data());

  EXPECT_STREQ("dim.", WeekDayShortLabel("fr_FR", kSunday).Utf8().data());
  EXPECT_STREQ("mer.", WeekDayShortLabel("fr_FR", kWednesday).Utf8().data());
  EXPECT_STREQ("sam.", WeekDayShortLabel("fr_FR", kSaturday).Utf8().data());

  EXPECT_STREQ("\xE6\x97\xA5",
               WeekDayShortLabel("ja_JP", kSunday).Utf8().data());
  EXPECT_STREQ("\xE6\xB0\xB4",
               WeekDayShortLabel("ja_JP", kWednesday).Utf8().data());
  EXPECT_STREQ("\xE5\x9C\x9F",
               WeekDayShortLabel("ja_JP", kSaturday).Utf8().data());
}

TEST_F(LocaleMacTest, isRTL) {
  EXPECT_TRUE(IsRTL("ar-eg"));
  EXPECT_FALSE(IsRTL("en-us"));
  EXPECT_FALSE(IsRTL("ja-jp"));
  EXPECT_FALSE(IsRTL("**invalid**"));
}

TEST_F(LocaleMacTest, monthFormat) {
  EXPECT_STREQ("MMMM yyyy", MonthFormat("en_US").Utf8().data());
  EXPECT_STREQ("yyyy\xE5\xB9\xB4M\xE6\x9C\x88",
               MonthFormat("ja_JP").Utf8().data());

  // fr_FR and ru return different results on OS versions.
  //  "MMM yyyy" "LLL yyyy" on 10.6 and 10.7
  //  "MMM y" "LLL y" on 10.8
}

TEST_F(LocaleMacTest, timeFormat) {
  EXPECT_STREQ("h:mm:ss a", TimeFormat("en_US").Utf8().data());
  EXPECT_STREQ("HH:mm:ss", TimeFormat("fr_FR").Utf8().data());
  EXPECT_STREQ("H:mm:ss", TimeFormat("ja_JP").Utf8().data());
}

TEST_F(LocaleMacTest, shortTimeFormat) {
  EXPECT_STREQ("h:mm a", ShortTimeFormat("en_US").Utf8().data());
  EXPECT_STREQ("HH:mm", ShortTimeFormat("fr_FR").Utf8().data());
  EXPECT_STREQ("H:mm", ShortTimeFormat("ja_JP").Utf8().data());
}

TEST_F(LocaleMacTest, standAloneMonthLabels) {
  EXPECT_STREQ("January",
               StandAloneMonthLabel("en_US", kJanuary).Utf8().data());
  EXPECT_STREQ("June", StandAloneMonthLabel("en_US", kJune).Utf8().data());
  EXPECT_STREQ("December",
               StandAloneMonthLabel("en_US", kDecember).Utf8().data());

  EXPECT_STREQ("janvier",
               StandAloneMonthLabel("fr_FR", kJanuary).Utf8().data());
  EXPECT_STREQ("juin", StandAloneMonthLabel("fr_FR", kJune).Utf8().data());
  EXPECT_STREQ(
      "d\xC3\xA9"
      "cembre",
      StandAloneMonthLabel("fr_FR", kDecember).Utf8().data());

  EXPECT_STREQ("1\xE6\x9C\x88",
               StandAloneMonthLabel("ja_JP", kJanuary).Utf8().data());
  EXPECT_STREQ("6\xE6\x9C\x88",
               StandAloneMonthLabel("ja_JP", kJune).Utf8().data());
  EXPECT_STREQ("12\xE6\x9C\x88",
               StandAloneMonthLabel("ja_JP", kDecember).Utf8().data());
}

TEST_F(LocaleMacTest, shortMonthLabels) {
  EXPECT_STREQ("Jan", ShortMonthLabel("en_US", 0).Utf8().data());
  EXPECT_STREQ("Jan", ShortStandAloneMonthLabel("en_US", 0).Utf8().data());
  EXPECT_STREQ("Dec", ShortMonthLabel("en_US", 11).Utf8().data());
  EXPECT_STREQ("Dec", ShortStandAloneMonthLabel("en_US", 11).Utf8().data());

  EXPECT_STREQ("janv.", ShortMonthLabel("fr_FR", 0).Utf8().data());
  EXPECT_STREQ("janv.", ShortStandAloneMonthLabel("fr_FR", 0).Utf8().data());
  EXPECT_STREQ(
      "d\xC3\xA9"
      "c.",
      ShortMonthLabel("fr_FR", 11).Utf8().data());
  EXPECT_STREQ(
      "d\xC3\xA9"
      "c.",
      ShortStandAloneMonthLabel("fr_FR", 11).Utf8().data());

  EXPECT_STREQ("1\xE6\x9C\x88", ShortMonthLabel("ja_JP", 0).Utf8().data());
  EXPECT_STREQ("1\xE6\x9C\x88",
               ShortStandAloneMonthLabel("ja_JP", 0).Utf8().data());
  EXPECT_STREQ("12\xE6\x9C\x88", ShortMonthLabel("ja_JP", 11).Utf8().data());
  EXPECT_STREQ("12\xE6\x9C\x88",
               ShortStandAloneMonthLabel("ja_JP", 11).Utf8().data());

  EXPECT_STREQ("\xD0\xBC\xD0\xB0\xD1\x80\xD1\x82\xD0\xB0",
               ShortMonthLabel("ru_RU", 2).Utf8().data());
  EXPECT_STREQ("\xD0\xBC\xD0\xB0\xD1\x8F",
               ShortMonthLabel("ru_RU", 4).Utf8().data());
  // The ru_RU locale returns different stand-alone month labels on OS versions.
  //  "\xD0\xBC\xD0\xB0\xD1\x80\xD1\x82" "\xD0\xBC\xD0\xB0\xD0\xB9" on 10.7
  //  "\xD0\x9C\xD0\xB0\xD1\x80\xD1\x82" "\xD0\x9C\xD0\xB0\xD0\xB9" on 10.8
}

TEST_F(LocaleMacTest, timeAMPMLabels) {
  EXPECT_STREQ("AM", TimeAMPMLabel("en_US", 0).Utf8().data());
  EXPECT_STREQ("PM", TimeAMPMLabel("en_US", 1).Utf8().data());

  EXPECT_STREQ("AM", TimeAMPMLabel("fr_FR", 0).Utf8().data());
  EXPECT_STREQ("PM", TimeAMPMLabel("fr_FR", 1).Utf8().data());

  EXPECT_STREQ("\xE5\x8D\x88\xE5\x89\x8D",
               TimeAMPMLabel("ja_JP", 0).Utf8().data());
  EXPECT_STREQ("\xE5\x8D\x88\xE5\xBE\x8C",
               TimeAMPMLabel("ja_JP", 1).Utf8().data());
}

TEST_F(LocaleMacTest, decimalSeparator) {
  EXPECT_STREQ(".", DecimalSeparator("en_US").Utf8().data());
  EXPECT_STREQ(",", DecimalSeparator("fr_FR").Utf8().data());
}

TEST_F(LocaleMacTest, invalidLocale) {
  EXPECT_STREQ(MonthLabel("en_US", kJanuary).Utf8().data(),
               MonthLabel("foo", kJanuary).Utf8().data());
  EXPECT_STREQ(DecimalSeparator("en_US").Utf8().data(),
               DecimalSeparator("foo").Utf8().data());
}

static void TestNumberIsReversible(const AtomicString& locale_string,
                                   const char* original,
                                   const char* should_have = 0) {
  std::unique_ptr<LocaleMac> locale = LocaleMac::Create(locale_string);
  String localized = locale->ConvertToLocalizedNumber(original);
  if (should_have)
    EXPECT_TRUE(localized.Contains(should_have));
  String converted = locale->ConvertFromLocalizedNumber(localized);
  EXPECT_STREQ(original, converted.Utf8().data());
}

void TestNumbers(const AtomicString& locale_string,
                 const char* decimal_separator_should_be = 0) {
  TestNumberIsReversible(locale_string, "123456789012345678901234567890");
  TestNumberIsReversible(locale_string, "-123.456",
                         decimal_separator_should_be);
  TestNumberIsReversible(locale_string, ".456", decimal_separator_should_be);
  TestNumberIsReversible(locale_string, "-0.456", decimal_separator_should_be);
}

TEST_F(LocaleMacTest, localizedNumberRoundTrip) {
  // Test some of major locales.
  TestNumbers("en_US", ".");
  TestNumbers("fr_FR", ",");
  TestNumbers("ar");
  TestNumbers("de_DE");
  TestNumbers("es_ES");
  TestNumbers("fa");
  TestNumbers("ja_JP");
  TestNumbers("ko_KR");
  TestNumbers("zh_CN");
  TestNumbers("zh_HK");
  TestNumbers("zh_TW");
}

}  // namespace blink
