// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"
#endif

namespace blink {

using ::testing::AtLeast;
using ::testing::Return;

class MockFontFormatCheck : public FontFormatCheck {
 public:
  explicit MockFontFormatCheck(sk_sp<SkData> data) : FontFormatCheck(data) {}
  MOCK_METHOD(bool, IsVariableFont, (), (const override));
  MOCK_METHOD(bool, IsCbdtCblcColorFont, (), (const override));
  MOCK_METHOD(bool, IsColrCpalColorFontV0, (), (const override));
  MOCK_METHOD(bool, IsColrCpalColorFontV1, (), (const override));
  MOCK_METHOD(bool, IsVariableColrV0Font, (), (const override));
  MOCK_METHOD(bool, IsSbixColorFont, (), (const override));
  MOCK_METHOD(bool, IsCff2OutlineFont, (), (const override));
};

sk_sp<SkTypeface> expect_called(sk_sp<SkData>) {
  EXPECT_TRUE(true);
  return nullptr;
}

sk_sp<SkTypeface> expect_not_called(sk_sp<SkData>) {
  EXPECT_FALSE(true);
  return nullptr;
}

const WebFontTypefaceFactory::FontInstantiator g_expect_system{
    .make_system = expect_called,
    .make_fontations = expect_not_called,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
    .make_fallback = expect_not_called,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
};

const WebFontTypefaceFactory::FontInstantiator g_expect_fontations{
    .make_system = expect_not_called,
    .make_fontations = expect_called,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
    .make_fallback = expect_not_called,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
};

#if BUILDFLAG(IS_WIN)
const WebFontTypefaceFactory::FontInstantiator g_expect_fallback{
    .make_system = expect_not_called,
    .make_fontations = expect_not_called,
    .make_fallback = expect_called,
};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)

TEST(WebFontTypefaceFactoryTest, DefaultAlwaysSystem) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsVariableFont()).Times(AtLeast(1));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_system);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedAlwaysColrV1) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsColrCpalColorFontV1())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_fontations);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedAlwaysCFF2) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsCff2OutlineFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_fontations);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedAlwaysCbdtCblc) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsCbdtCblcColorFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_fontations);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedVariableSystem) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);

  sk_sp<SkTypeface> out_typeface;
#if BUILDFLAG(IS_WIN)
  const WebFontTypefaceFactory::FontInstantiator& expectation =
      DWriteVersionSupportsVariations() ? g_expect_system : g_expect_fallback;
#else
  const WebFontTypefaceFactory::FontInstantiator& expectation = g_expect_system;
#endif
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check, expectation);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedStaticSystem) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);

  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_system);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedVariableColrV0) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsColrCpalColorFontV0())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_font_format_check, IsVariableFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;

#if BUILDFLAG(IS_WIN)
  const WebFontTypefaceFactory::FontInstantiator& expectation =
      DWriteVersionSupportsVariations() ? g_expect_system : g_expect_fontations;
#else
  const WebFontTypefaceFactory::FontInstantiator& expectation =
      g_expect_fontations;
#endif
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check, expectation);
}

TEST(WebFontTypefaceFactoryTest, FontationsSelectedSbixNonApple) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsSbixColorFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;

#if BUILDFLAG(IS_APPLE)
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_system);
#else
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expect_fontations);
#endif
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1499557): Currently fails on the platform.
#define MAYBE_VariationsWinFallbackIfNeeded \
  DISABLED_VariationsWinFallbackIfNeeded
#else
#define MAYBE_VariationsWinFallbackIfNeeded VariationsWinFallbackIfNeeded
#endif
TEST(WebFontTypefaceFactoryTest, MAYBE_VariationsWinFallbackIfNeeded) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsVariableFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;

#if BUILDFLAG(IS_WIN)
  const WebFontTypefaceFactory::FontInstantiator& expectation =
      DWriteVersionSupportsVariations() ? g_expect_system : g_expect_fallback;
#else
  const WebFontTypefaceFactory::FontInstantiator& expectation = g_expect_system;
#endif
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check, expectation);
}

}  // namespace blink
