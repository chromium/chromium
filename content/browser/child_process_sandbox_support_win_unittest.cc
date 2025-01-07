// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/sandbox_support_impl.h"
#include "content/common/sandbox_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
using LcTypeString = mojom::SandboxSupport::LcTypeString;
using LcTypeStrings = mojom::SandboxSupport::LcTypeStrings;

namespace {

inline constexpr uint32_t kSunday = 0;

// LCIDs - See https://msdn.microsoft.com/en-us/goglobal/bb964664.aspx
inline constexpr uint32_t kEnglishUS = 0x409;       // en-us
inline constexpr uint32_t kSpanishMexico = 0x080A;  // es-MX
inline constexpr uint32_t kKoreanKorea = 0x0412;    // ko-KR

// Test that the mojo interface calls the expected Windows APIs.
class SandboxSupportWinUnitTest : public testing::Test {
 public:
  SandboxSupportWinUnitTest() {
    features_.InitFromCommandLine("WinSboxProxyLocale", "");
    impl_.BindReceiver(sandbox_support_.BindNewPipeAndPassReceiver());
  }

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &SandboxSupportWinUnitTest::OnProcessError, base::Unretained(this)));
  }

  content::mojom::SandboxSupport& sandbox_support() {
    return *sandbox_support_;
  }

  bool had_error() { return had_error_; }
  void OnProcessError(const std::string& error) { had_error_ = true; }

  bool had_error_ = false;
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<content::mojom::SandboxSupport> sandbox_support_;
  SandboxSupportImpl impl_;
};
}  // namespace

TEST_F(SandboxSupportWinUnitTest, BasicEnUs) {
  uint32_t lcid;
  uint32_t first_day;
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"en-us", u"en-us", true,
                                                      &lcid, &first_day));
  EXPECT_EQ(first_day, kSunday);
  // This depends on the system configuration.
  EXPECT_TRUE(lcid == LOCALE_USER_DEFAULT || lcid == kEnglishUS);

  uint32_t digit_sub;
  std::u16string digits;
  std::u16string decimal;
  std::u16string thousand;
  std::u16string negative_sign;
  uint32_t negnumber;
  EXPECT_TRUE(sandbox_support().DigitsAndSigns(lcid, true, &digit_sub, &digits,
                                               &decimal, &thousand,
                                               &negative_sign, &negnumber));
  EXPECT_EQ(digit_sub, 1u);
  EXPECT_TRUE(digits.empty());
  EXPECT_EQ(decimal, u".");
  EXPECT_EQ(thousand, u",");
  EXPECT_EQ(negative_sign, u"-");
  EXPECT_EQ(negnumber, 1u);

  std::u16string tmp_string;
  EXPECT_TRUE(sandbox_support().LocaleString(
      lcid, true, LcTypeString::kYearMonth, &tmp_string));
  EXPECT_EQ(tmp_string, u"MMMM yyyy");
  std::vector<std::u16string> tmp_strings;
  EXPECT_TRUE(sandbox_support().LocaleStrings(lcid, true, LcTypeStrings::kAmPm,
                                              &tmp_strings));
  EXPECT_EQ(tmp_strings.size(), 2u);
  EXPECT_EQ(tmp_strings.at(1), u"PM");
}

TEST_F(SandboxSupportWinUnitTest, Locales) {
  uint32_t lcid;
  uint32_t first_day;
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"es-MX", u"es-MX", true,
                                                      &lcid, &first_day));
  EXPECT_EQ(lcid, kSpanishMexico);
  EXPECT_EQ(first_day, kSunday);
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"ko-KR", u"ko-KR", true,
                                                      &lcid, &first_day));
  EXPECT_EQ(lcid, kKoreanKorea);
  EXPECT_EQ(first_day, kSunday);
}

TEST_F(SandboxSupportWinUnitTest, NonDefault) {
  // Chrome actually calls these with `force_defaults=false`. We cannot test
  // return values as the test machine may have custom locales or settings.
  uint32_t lcid;
  uint32_t first_day;
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"en-us", u"en-us", false,
                                                      &lcid, &first_day));
  EXPECT_TRUE(lcid == LOCALE_USER_DEFAULT || lcid == kEnglishUS);
  uint32_t digit_sub;
  std::u16string digits;
  std::u16string decimal;
  std::u16string thousand;
  std::u16string negative_sign;
  uint32_t negnumber;
  EXPECT_TRUE(sandbox_support().DigitsAndSigns(lcid, true, &digit_sub, &digits,
                                               &decimal, &thousand,
                                               &negative_sign, &negnumber));
  std::u16string tmp_string;
  EXPECT_TRUE(sandbox_support().LocaleString(
      lcid, true, LcTypeString::kYearMonth, &tmp_string));
  EXPECT_EQ(tmp_string, u"MMMM yyyy");
  std::vector<std::u16string> tmp_strings;
  EXPECT_TRUE(sandbox_support().LocaleStrings(lcid, true, LcTypeStrings::kAmPm,
                                              &tmp_strings));
}

TEST_F(SandboxSupportWinUnitTest, MultiStrings) {
  // Collection => expected value of 2nd element. (kAmPm is shortest with two.)
  std::vector<std::pair<LcTypeStrings, std::u16string>> expected = {
      {LcTypeStrings::kMonths, u"February"},
      {LcTypeStrings::kShortMonths, u"Feb"},
      {LcTypeStrings::kShortWeekDays, u"Mo"},
      {LcTypeStrings::kAmPm, u"PM"}};
  uint32_t lcid;
  uint32_t first_day;
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"en-us", u"en-us", false,
                                                      &lcid, &first_day));
  EXPECT_TRUE(lcid == LOCALE_USER_DEFAULT || lcid == kEnglishUS);

  for (const auto& val : expected) {
    std::vector<std::u16string> tmp_strings;
    EXPECT_TRUE(
        sandbox_support().LocaleStrings(lcid, true, val.first, &tmp_strings));
    EXPECT_EQ(val.second, tmp_strings.at(1));
  }
}

TEST_F(SandboxSupportWinUnitTest, Strings) {
  // Collection => expected value of item.
  std::vector<std::pair<LcTypeString, std::u16string>> expected = {
      {LcTypeString::kShortDate, u"M/d/yyyy"},
      {LcTypeString::kYearMonth, u"MMMM yyyy"},
      {LcTypeString::kTimeFormat, u"h:mm:ss tt"},
      {LcTypeString::kShortTime, u"h:mm tt"}};
  uint32_t lcid;
  uint32_t first_day;
  EXPECT_TRUE(sandbox_support().LcidAndFirstDayOfWeek(u"en-us", u"en-us", false,
                                                      &lcid, &first_day));
  EXPECT_TRUE(lcid == LOCALE_USER_DEFAULT || lcid == kEnglishUS);

  for (const auto& val : expected) {
    std::u16string tmp_string;
    EXPECT_TRUE(
        sandbox_support().LocaleString(lcid, true, val.first, &tmp_string));
    EXPECT_EQ(val.second, tmp_string);
  }
}

TEST_F(SandboxSupportWinUnitTest, LimitsLanguage) {
  uint32_t lcid;
  uint32_t first_day;
  // https://learn.microsoft.com/en-us/windows/win32/intl/locale-name-constants
  // LOCALE_NAME_MAX_LENGTH at time of writing was 85.
  static_assert(LOCALE_NAME_MAX_LENGTH <= 85);
  EXPECT_FALSE(sandbox_support().LcidAndFirstDayOfWeek(
      u"0123456789012345678901234567890123456789"
      u"0123456789012345678901234567890123456789tooooooooolong-"
      u"toolongtoolong",
      u"en-us", false, &lcid, &first_day));
  EXPECT_TRUE(had_error());
}

TEST_F(SandboxSupportWinUnitTest, LimitsDefaultLanguage) {
  uint32_t lcid;
  uint32_t first_day;
  // https://learn.microsoft.com/en-us/windows/win32/intl/locale-name-constants
  // LOCALE_NAME_MAX_LENGTH at time of writing was 85.
  static_assert(LOCALE_NAME_MAX_LENGTH <= 85);
  EXPECT_FALSE(sandbox_support().LcidAndFirstDayOfWeek(
      u"en-us",
      u"0123456789012345678901234567890123456789"
      u"0123456789012345678901234567890123456789tooooooooolong-"
      u"toolongtoolong",
      false, &lcid, &first_day));
  EXPECT_TRUE(had_error());
}

}  // namespace content
