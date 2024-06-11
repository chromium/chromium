// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"

#include <testing/gmock/include/gmock/gmock.h>
#include <testing/gtest/include/gtest/gtest.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

Font CreateNotoCjk() {
  return blink::test::CreateTestFont(
      AtomicString("Noto Sans CJK"),
      blink::test::BlinkWebTestsFontsTestDataPath(
          "noto/cjk/NotoSansCJKjp-Regular-subset-halt.otf"),
      16.0);
}

class HanKerningTest : public testing::Test {};

TEST_F(HanKerningTest, MayApply) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, true);

  for (UChar32 ch = 0; ch < kMaxCodepoint; ++ch) {
    StringBuilder builder;
    builder.Append(ch);
    String text = builder.ToString();

    for (wtf_size_t i = 0; i < text.length(); ++i) {
      const HanKerning::CharType type =
          HanKerning::GetCharType(text[i], ja_data);
      if (type == HanKerning::CharType::kOpen ||
          type == HanKerning::CharType::kOpenQuote ||
          type == HanKerning::CharType::kClose ||
          type == HanKerning::CharType::kCloseQuote) {
        EXPECT_EQ(HanKerning::MayApply(text), true)
            << String::Format("U+%06X", ch);
        break;
      }
    }
  }
}

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

  // Quote characters are proportional for Japanese, fullwidth for Chinese.
  EXPECT_FALSE(ja_data.is_quote_fullwidth);
  EXPECT_TRUE(zhs_data.is_quote_fullwidth);
  EXPECT_TRUE(zht_data.is_quote_fullwidth);
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

  // Quote characters are fullwidth when vertical upright, but Japanese
  // placement is different from expected.
  EXPECT_FALSE(ja_data.is_quote_fullwidth);
  EXPECT_TRUE(zhs_data.is_quote_fullwidth);
  EXPECT_TRUE(zht_data.is_quote_fullwidth);
}

#if BUILDFLAG(IS_WIN)
// A test case of CJK fullwidth punctuation has slightly different widths from
// the `IdeographicInlineSize` (the width of U+0x6C34). crbug.com/1519775
// https://collabo-cafe.com/events/collabo/shingeki-anime-completed-hajime-isayama-illust2023/
TEST_F(HanKerningTest, FontDataSizeError) {
  class EnableAntialiasedText {
   public:
    EnableAntialiasedText()
        : is_antialiased_text_enabled_(
              FontCache::Get().AntialiasedTextEnabled()) {
      FontCache::Get().SetAntialiasedTextEnabled(true);
    }
    ~EnableAntialiasedText() {
      FontCache::Get().SetAntialiasedTextEnabled(is_antialiased_text_enabled_);
    }

   private:
    bool is_antialiased_text_enabled_;
  } enable_antialias_text;

  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(AtomicString("Yu Gothic"), FontFamily::Type::kFamilyName));
  const float specified_size = 16.f * 1.03f;
  font_description.SetSpecifiedSize(specified_size);
  const float computed_size = specified_size * 1.25f;
  font_description.SetComputedSize(computed_size);
  font_description.SetFontSmoothing(FontSmoothingMode::kAntialiased);
  Font font(font_description);
  const SimpleFontData* primary_font = font.PrimaryFont();

  SkString name;
  primary_font->PlatformData().Typeface()->getPostScriptName(&name);
  if (!name.equals("YuGothic-Regular")) {
    return;
  }

  scoped_refptr<LayoutLocale> locale =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  HanKerning::FontData data(*font.PrimaryFont(), *locale, true);
  EXPECT_TRUE(data.has_alternate_spacing);
  EXPECT_EQ(data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(data.type_for_semicolon, HanKerning::CharType::kMiddle);
  EXPECT_FALSE(data.is_quote_fullwidth);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(HanKerningTest, ResetFeatures) {
  Font noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk.PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  FontFeatures features;
  features.Append(
      {HB_TAG('T', 'E', 'S', 'T'), 1, 0, static_cast<unsigned>(-1)});
  EXPECT_EQ(features.size(), 1u);
  const String text(u"国）（国");
  {
    HanKerning han_kerning(text, 0, text.length(), *noto_cjk_data,
                           noto_cjk.GetFontDescription(), HanKerning::Options(),
                           &features);
    EXPECT_EQ(features.size(), 2u);
  }
  EXPECT_EQ(features.size(), 1u);
}

}  // namespace blink
