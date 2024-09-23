// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
bool HasFontWithName(const base::Value::List& list,
                     std::string_view expected_font_id,
                     std::string_view expected_display_name) {
  for (const auto& font : list) {
    const auto& font_names = font.GetList();
    std::string font_id = font_names[0].GetString();
    std::string display_name = font_names[1].GetString();
    if (font_id == expected_font_id && display_name == expected_display_name)
      return true;
  }

  return false;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
// GetFontList is not implemented on Android and Fuchsia.
TEST(FontList, GetFontList) {
  base::test::TaskEnvironment task_environment;

  content::GetFontListTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([] {
        base::Value::List fonts = content::GetFontList_SlowBlocking();

#if BUILDFLAG(IS_WIN)
        EXPECT_TRUE(HasFontWithName(fonts, "MS Gothic", "MS Gothic"));
        EXPECT_TRUE(HasFontWithName(fonts, "Segoe UI", "Segoe UI"));
        EXPECT_TRUE(HasFontWithName(fonts, "Verdana", "Verdana"));
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        EXPECT_TRUE(HasFontWithName(fonts, "Arimo", "Arimo"));
#else
        EXPECT_TRUE(HasFontWithName(fonts, "Arial", "Arial"));
#endif
      }));
  task_environment.RunUntilIdle();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
TEST(FontList, GetFontListLocalized) {
  base::i18n::SetICUDefaultLocale("ja-JP");
  base::Value::List ja_fonts = content::GetFontList_SlowBlocking();
  EXPECT_TRUE(HasFontWithName(ja_fonts, "MS Gothic", "ＭＳ ゴシック"));

  base::i18n::SetICUDefaultLocale("ko-KR");
  base::Value::List ko_fonts = content::GetFontList_SlowBlocking();
  EXPECT_TRUE(HasFontWithName(ko_fonts, "Malgun Gothic", "맑은 고딕"));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
// On some macOS versions, CTFontManager returns LastResort and/or hidden fonts.
// Ensure that someone (CTFontManager or our FontList code) filters these fonts
// on all OS versions that we support.
TEST(FontList, GetFontListDoesNotIncludeHiddenFonts) {
  base::Value::List fonts = content::GetFontList_SlowBlocking();

  for (const auto& font : fonts) {
    const auto& font_names = font.GetList();
    const std::string& font_id = font_names[0].GetString();

    // The checks are inspired by Gecko's gfxMacPlatformFontList::AddFamily.
    EXPECT_FALSE(base::EqualsCaseInsensitiveASCII(font_id, "lastresort"))
        << font_id << " seems to be LastResort, which should be filtered";
    EXPECT_FALSE(font_id[0] == '.')
        << font_id << " seems like a hidden font, which should be filtered";
  }
}
#endif  // BUILDFLAG(IS_MAC)
