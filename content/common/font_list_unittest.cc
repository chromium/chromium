// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
bool HasFontWithName(const base::ListValue& list,
                     base::StringPiece expected_font_id,
                     base::StringPiece expected_display_name) {
  for (const auto& font : list.GetList()) {
    const auto& font_names = font.GetList();
    std::string font_id = font_names[0].GetString();
    std::string display_name = font_names[1].GetString();
    if (font_id == expected_font_id && display_name == expected_display_name)
      return true;
  }

  return false;
}
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSI

}  // namespace

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
// GetFontList is not implemented on Android and Fuchsia.
TEST(FontList, GetFontList) {
  base::test::TaskEnvironment task_environment;

  content::GetFontListTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([] {
        std::unique_ptr<base::ListValue> fonts =
            content::GetFontList_SlowBlocking();
        ASSERT_TRUE(fonts);

#if defined(OS_WIN)
        EXPECT_TRUE(HasFontWithName(*fonts, "MS Gothic", "MS Gothic"));
        EXPECT_TRUE(HasFontWithName(*fonts, "Segoe UI", "Segoe UI"));
        EXPECT_TRUE(HasFontWithName(*fonts, "Verdana", "Verdana"));
#elif defined(OS_LINUX)
        EXPECT_TRUE(HasFontWithName(*fonts, "Arimo", "Arimo"));
#else
        EXPECT_TRUE(HasFontWithName(*fonts, "Arial", "Arial"));
#endif
      }));
  task_environment.RunUntilIdle();
}
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#if defined(OS_WIN)
TEST(FontList, GetFontListLocalized) {
  base::i18n::SetICUDefaultLocale("ja-JP");
  std::unique_ptr<base::ListValue> ja_fonts =
      content::GetFontList_SlowBlocking();
  ASSERT_TRUE(ja_fonts);
  EXPECT_TRUE(HasFontWithName(*ja_fonts, "MS Gothic", "ＭＳ ゴシック"));

  base::i18n::SetICUDefaultLocale("ko-KR");
  std::unique_ptr<base::ListValue> ko_fonts =
      content::GetFontList_SlowBlocking();
  ASSERT_TRUE(ko_fonts);
  EXPECT_TRUE(HasFontWithName(*ko_fonts, "Malgun Gothic", "맑은 고딕"));
}
#endif  // defined(OS_WIN)
