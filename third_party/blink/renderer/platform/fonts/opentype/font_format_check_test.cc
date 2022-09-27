// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FontFormatCheckTest : public testing::Test {
 protected:
  void EnsureFontData(String font_file_name) {
    sk_sp<SkData> font_file_data(SkData::MakeFromFileName(
        test::PlatformTestDataPath(font_file_name).Utf8().data()));
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

}  // namespace blink
