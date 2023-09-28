// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"

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

class HanKerningTest : public testing::Test {};

TEST_F(HanKerningTest, FontDataHorizontal) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, true);
  HanKerning::FontData zhs_data(*noto_cjk_data, *zhs, true);
  HanKerning::FontData zht_data(*noto_cjk_data, *zht, true);

  // In the Adobe's common convention:
  // * Place full stop and comma at center only for Traditional Chinese.
  // * Place colon and semicolon on the left only for Simplified Chinese.
  EXPECT_EQ(ja_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, HanKerning::CharType::kMiddle);

  EXPECT_EQ(ja_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(ja_data.type_for_semicolon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(zhs_data.type_for_colon, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_semicolon, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(zht_data.type_for_semicolon, HanKerning::CharType::kMiddle);
}

TEST_F(HanKerningTest, FontDataVertical) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, false);
  HanKerning::FontData zhs_data(*noto_cjk_data, *zhs, false);
  HanKerning::FontData zht_data(*noto_cjk_data, *zht, false);

  EXPECT_EQ(ja_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, HanKerning::CharType::kMiddle);

  // In the Adobe's common convention, only colon in Japanese rotates, and all
  // other cases are upright.
  EXPECT_EQ(ja_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(ja_data.type_for_semicolon, HanKerning::CharType::kOther);
  EXPECT_EQ(zhs_data.type_for_colon, HanKerning::CharType::kOther);
  EXPECT_EQ(zhs_data.type_for_semicolon, HanKerning::CharType::kOther);
  EXPECT_EQ(zht_data.type_for_colon, HanKerning::CharType::kOther);
  EXPECT_EQ(zht_data.type_for_semicolon, HanKerning::CharType::kOther);
}

}  // namespace

}  // namespace blink
