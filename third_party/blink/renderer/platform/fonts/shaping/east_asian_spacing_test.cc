// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/east_asian_spacing.h"

#include <testing/gmock/include/gmock/gmock.h>
#include <testing/gtest/include/gtest/gtest.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

Font CreateNotoCjk() {
  return blink::test::CreateTestFont(
      AtomicString("Noto Sans CJK"),
      blink::test::BlinkWebTestsFontsTestDataPath(
          "noto/cjk/NotoSansCJKjp-Regular-subset-chws.otf"),
      16.0);
}

class EastAsianSpacingTest : public testing::Test {};

TEST_F(EastAsianSpacingTest, FontDataHorizontal) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  EastAsianSpacing::FontData ja_data(*noto_cjk_data, *ja, true);
  EastAsianSpacing::FontData zhs_data(*noto_cjk_data, *zhs, true);
  EastAsianSpacing::FontData zht_data(*noto_cjk_data, *zht, true);

  // In the Adobe's common convention:
  // * Place full stop and comma at center only for Traditional Chinese.
  // * Place colon and semicolon on the left only for Simplified Chinese.
  EXPECT_EQ(ja_data.type_for_dot, EastAsianSpacing::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, EastAsianSpacing::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, EastAsianSpacing::CharType::kMiddle);

  EXPECT_EQ(ja_data.type_for_colon, EastAsianSpacing::CharType::kMiddle);
  EXPECT_EQ(zhs_data.type_for_colon, EastAsianSpacing::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_colon, EastAsianSpacing::CharType::kMiddle);
}

TEST_F(EastAsianSpacingTest, FontDataVertical) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  EastAsianSpacing::FontData ja_data(*noto_cjk_data, *ja, false);
  EastAsianSpacing::FontData zhs_data(*noto_cjk_data, *zhs, false);
  EastAsianSpacing::FontData zht_data(*noto_cjk_data, *zht, false);

  EXPECT_EQ(ja_data.type_for_dot, EastAsianSpacing::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, EastAsianSpacing::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, EastAsianSpacing::CharType::kMiddle);

  // In the Adobe's common convention, only colon in Japanese rotates, and all
  // other cases are upright.
  EXPECT_EQ(ja_data.type_for_colon, EastAsianSpacing::CharType::kOther);
  EXPECT_EQ(zhs_data.type_for_colon, EastAsianSpacing::CharType::kOther);
  EXPECT_EQ(zht_data.type_for_colon, EastAsianSpacing::CharType::kOther);
}

}  // namespace

}  // namespace blink
