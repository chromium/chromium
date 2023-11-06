// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
WebFontTypefaceFactory::FontInstantiator g_expectSystem{expect_called,
                                                        expect_not_called};
WebFontTypefaceFactory::FontInstantiator g_expectFallback{expect_not_called,
                                                          expect_called};
#else
WebFontTypefaceFactory::FontInstantiator g_expectSystem{expect_called};
#endif

TEST(WebFontTypefaceFactoryTest, DefaultAlwaysSystem) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsVariableFont()).Times(AtLeast(1));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
                                         g_expectSystem);
}

TEST(WebFontTypefaceFactoryTest, ColrV1AlwaysFallback) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsColrCpalColorFontV1())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
                                         g_expectFallback
#else
                                         g_expectSystem
#endif
  );
}

TEST(WebFontTypefaceFactoryTest, Cff2AlwaysFallback) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsCff2OutlineFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
                                         g_expectFallback
#else
                                         g_expectSystem
#endif
  );
}

TEST(WebFontTypefaceFactoryTest, CbdtCblcAlwaysFallback) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsCbdtCblcColorFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
                                         g_expectFallback
#else
                                         g_expectSystem
#endif
  );
}

TEST(WebFontTypefaceFactoryTest, ColrV0FallbackApple) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsColrCpalColorFontV0())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
#if BUILDFLAG(IS_APPLE)
                                         g_expectFallback
#else
                                         g_expectSystem
#endif
  );
}

TEST(WebFontTypefaceFactoryTest, VariableColrV0FallbackWindowsApple) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsColrCpalColorFontV0())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_font_format_check, IsVariableFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::FontInstantiator& expectation = g_expectSystem;
#if BUILDFLAG(IS_WIN)
  if (!DWriteVersionSupportsVariations()) {
    expectation = g_expectFallback;
  }
#elif BUILDFLAG(IS_APPLE)
  expectation = g_expectFallback;
#endif
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check, expectation);
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1499557): Currently fails on the platform.
#define MAYBE_SbixFallbackWindows DISABLED_SbixFallbackWindows
#else
#define MAYBE_SbixFallbackWindows SbixFallbackWindows
#endif
TEST(WebFontTypefaceFactoryTest, MAYBE_SbixFallbackWindows) {
  sk_sp<SkData> data = SkData::MakeEmpty();
  MockFontFormatCheck mock_font_format_check(data);
  EXPECT_CALL(mock_font_format_check, IsSbixColorFont())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  sk_sp<SkTypeface> out_typeface;
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check,
#if BUILDFLAG(IS_WIN)
                                         g_expectFallback
#else
                                         g_expectSystem
#endif
  );
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

  WebFontTypefaceFactory::FontInstantiator& expectation = g_expectSystem;
#if BUILDFLAG(IS_WIN)
  if (!DWriteVersionSupportsVariations()) {
    expectation = g_expectFallback;
  }
#endif
  WebFontTypefaceFactory::CreateTypeface(SkData::MakeEmpty(), out_typeface,
                                         mock_font_format_check, expectation);
}

}  // namespace blink
