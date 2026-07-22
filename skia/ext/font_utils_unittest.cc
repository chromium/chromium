// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/font_utils.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace skia {
namespace {

TEST(SkiaFontUtilsTest, SanitizeTypefaceStream_NullOrEmpty) {
  EXPECT_FALSE(SanitizeTypefaceStream(nullptr));
  EXPECT_FALSE(SanitizeTypefaceStream(SkData::MakeEmpty()));
}

TEST(SkiaFontUtilsTest, SanitizeTypefaceStream_InvalidData) {
  const char kJunk[] = "invalid_font_data_junk_bytes";
  sk_sp<SkData> junk_data = SkData::MakeWithCopy(kJunk, sizeof(kJunk));
  EXPECT_FALSE(SanitizeTypefaceStream(junk_data));
}

TEST(SkiaFontUtilsTest, SanitizeTypefaceStream_TooLarge) {
  // 128 MB + 1 byte
  constexpr size_t kTooLargeSize = 128 * 1024 * 1024 + 1;
  sk_sp<SkData> large_data = SkData::MakeUninitialized(kTooLargeSize);
  EXPECT_FALSE(SanitizeTypefaceStream(large_data));
}

TEST(SkiaFontUtilsTest, SanitizeTypefaceStream_ValidFont) {
  auto default_typeface = DefaultTypeface();
  ASSERT_TRUE(default_typeface);
  int ttc_index = 0;
  std::unique_ptr<SkStreamAsset> stream =
      default_typeface->openStream(&ttc_index);
  if (!stream) {
    return;
  }
  sk_sp<SkData> original_data =
      SkData::MakeFromStream(stream.get(), stream->getLength());
  ASSERT_TRUE(original_data);

  sk_sp<SkData> sanitized_data = SanitizeTypefaceStream(original_data);
  EXPECT_TRUE(sanitized_data);
  EXPECT_GT(sanitized_data->size(), 0u);
}

}  // namespace
}  // namespace skia
