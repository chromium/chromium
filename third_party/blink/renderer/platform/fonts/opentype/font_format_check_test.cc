// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

#include "base/containers/heap_array.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

class FontFormatCheckTest : public testing::Test {
 protected:
  void EnsureFontData(String font_file_name) {
    EnsureFontDataFromPath(test::PlatformTestDataPath(font_file_name));
  }

  void EnsureFontDataFromPath(const String& font_file_path) {
    sk_sp<SkData> font_file_data(
        SkData::MakeFromFileName(font_file_path.Utf8().data()));
    ASSERT_FALSE(font_file_data->isEmpty());
    font_data_ = font_file_data;
  }

  sk_sp<SkData> font_data_;
};

TEST_F(FontFormatCheckTest, NoCOLR) {
  EnsureFontData("roboto-a.ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_FALSE(format_check.IsColrCpalColorFontV0());
  ASSERT_FALSE(format_check.IsColrCpalColorFontV1());
}

TEST_F(FontFormatCheckTest, COLRV1) {
  EnsureFontData("colrv1_test.ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_TRUE(format_check.IsColrCpalColorFontV1());
  ASSERT_FALSE(format_check.IsColrCpalColorFontV0());
}

TEST_F(FontFormatCheckTest, COLRV0) {
  EnsureFontData("colrv0_test.ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_TRUE(format_check.IsColrCpalColorFontV0());
  ASSERT_FALSE(format_check.IsColrCpalColorFontV1());
}

TEST_F(FontFormatCheckTest, EbdtEblc) {
  EnsureFontDataFromPath(test::BlinkWebTestsDir() +
                         "/resources/EbdtMono/ebdt_fmt1.ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_TRUE(format_check.IsEbdtEblcMonochromeFont());
  ASSERT_FALSE(format_check.IsCbdtCblcColorFont());
  ASSERT_FALSE(format_check.IsColrCpalColorFontV0());
  ASSERT_FALSE(format_check.IsColrCpalColorFontV1());
}

TEST_F(FontFormatCheckTest, NoEbdtEblc) {
  EnsureFontData("roboto-a.ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_FALSE(format_check.IsEbdtEblcMonochromeFont());
}

TEST_F(FontFormatCheckTest, Avar1) {
  EnsureFontDataFromPath(test::BlinkWebTestsDir() +
                         "/external/wpt/css/css-fonts/resources/avar/"
                         "DesignSpaceWarpTestAvar1[opsz,wdth,wght].ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_FALSE(format_check.IsAvar2Font());
}

TEST_F(FontFormatCheckTest, Avar2) {
  EnsureFontDataFromPath(test::BlinkWebTestsDir() +
                         "/external/wpt/css/css-fonts/resources/avar/"
                         "DesignSpaceWarpTestAvar2[opsz,wdth,wght].ttf");
  FontFormatCheck format_check(font_data_);
  ASSERT_TRUE(format_check.IsAvar2Font());
}

TEST_F(FontFormatCheckTest, AvarVersion3) {
  EnsureFontDataFromPath(test::BlinkWebTestsDir() +
                         "/external/wpt/css/css-fonts/resources/avar/"
                         "DesignSpaceWarpTestAvar2[opsz,wdth,wght].ttf");
  auto modified_font_bytes =
      base::HeapArray<uint8_t>::CopiedFrom(skia::as_byte_span(*font_data_));

  // Locate the 'avar' table directory entry and modify its major version to 3.
  constexpr uint32_t kAvarTag = SkSetFourByteTag('a', 'v', 'a', 'r');
  size_t avar_offset = 0;
  uint16_t num_tables = (modified_font_bytes[4] << 8) | modified_font_bytes[5];
  for (uint16_t i = 0; i < num_tables; ++i) {
    size_t record_offset = 12 + i * 16;
    uint32_t tag = SkSetFourByteTag(modified_font_bytes[record_offset],
                                    modified_font_bytes[record_offset + 1],
                                    modified_font_bytes[record_offset + 2],
                                    modified_font_bytes[record_offset + 3]);
    if (tag == kAvarTag) {
      avar_offset = (modified_font_bytes[record_offset + 8] << 24) |
                    (modified_font_bytes[record_offset + 9] << 16) |
                    (modified_font_bytes[record_offset + 10] << 8) |
                    modified_font_bytes[record_offset + 11];
      break;
    }
  }
  ASSERT_NE(avar_offset, 0u);
  ASSERT_EQ(modified_font_bytes[avar_offset], 0);
  ASSERT_EQ(modified_font_bytes[avar_offset + 1], 2);
  modified_font_bytes[avar_offset + 1] = 3;

  sk_sp<SkData> modified_data =
      gfx::MakeSkDataFromSpanWithCopy(modified_font_bytes);
  FontFormatCheck format_check(modified_data);
  ASSERT_TRUE(format_check.IsAvar2Font());
}

}  // namespace blink
