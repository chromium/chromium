// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"

#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

scoped_refptr<SharedBuffer> LoadFont(const String& path) {
  std::optional<Vector<char>> font_data =
      test::ReadFromFile(test::PlatformTestDataPath(path));
  CHECK(font_data.has_value());
  return SharedBuffer::Create(std::move(*font_data));
}

}  // namespace

// Regression test for a font that triggers a large number of OTS warnings
// during decoding. Without a bound on the accumulated error string, processing
// these warnings dominated decode time and made the OpenType math fuzzer time
// out. Verify that the reported error string stays within the budget.
TEST(WebFontDecoderTest, ErrorStringIsBoundedOnPathologicalFont) {
  scoped_refptr<SharedBuffer> font_buffer =
      LoadFont("open_type_math_support_fuzzer_timeout.ttf");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  // Messages are accepted until the accumulated string reaches the ~4096 byte
  // budget, so the result may overshoot by at most one final message. The
  // point of the bound is that the string stays small instead of growing
  // without limit (which is what caused the fuzzer timeout), so allow generous
  // slack over the budget for that trailing message.
  ASSERT_FALSE(decoded_result.has_value());
  EXPECT_LT(decoded_result.error().length(), 8192u);
}

TEST(WebFontDecoderTest, EmptyBufferIsError) {
  scoped_refptr<SharedBuffer> font_buffer = SharedBuffer::Create();
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());
  ASSERT_FALSE(decoded_result.has_value());
}

TEST(WebFontDecoderTest, IftWoff2WithFeatureEnabledThenCreatesIftPatcher) {
  ScopedIncrementalFontTransferForTest scoped_ift(true);
  scoped_refptr<SharedBuffer> font_buffer = LoadFont("roboto-ift.woff2");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  ASSERT_TRUE(decoded_result.has_value());
  EXPECT_NE(decoded_result->sk_typeface, nullptr);
  EXPECT_NE(decoded_result->ift_patcher, nullptr);
  EXPECT_GT(decoded_result->decoded_size, 0u);
}

TEST(WebFontDecoderTest, IftTtfWithFeatureEnabledThenCreatesIftPatcher) {
  ScopedIncrementalFontTransferForTest scoped_ift(true);
  scoped_refptr<SharedBuffer> font_buffer = LoadFont("roboto-ift.ttf");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  ASSERT_TRUE(decoded_result.has_value());
  EXPECT_NE(decoded_result->sk_typeface, nullptr);
  EXPECT_NE(decoded_result->ift_patcher, nullptr);
  EXPECT_GT(decoded_result->decoded_size, 0u);
}

TEST(WebFontDecoderTest, IftFontWithFeatureDisabledThenIftPatcherIsNull) {
  ScopedIncrementalFontTransferForTest scoped_ift(false);
  scoped_refptr<SharedBuffer> font_buffer = LoadFont("roboto-ift.woff2");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  ASSERT_TRUE(decoded_result.has_value());
  EXPECT_NE(decoded_result->sk_typeface, nullptr);
  EXPECT_EQ(decoded_result->ift_patcher, nullptr);
  EXPECT_GT(decoded_result->decoded_size, 0u);
}

TEST(WebFontDecoderTest, NonIftFontWithFeatureEnabledThenIftPatcherIsNull) {
  ScopedIncrementalFontTransferForTest scoped_ift(true);
  scoped_refptr<SharedBuffer> font_buffer =
      LoadFont("third_party/Roboto/roboto-regular.woff2");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  ASSERT_TRUE(decoded_result.has_value());
  EXPECT_NE(decoded_result->sk_typeface, nullptr);
  EXPECT_EQ(decoded_result->ift_patcher, nullptr);
  EXPECT_GT(decoded_result->decoded_size, 0u);
}

TEST(WebFontDecoderTest, WoffFontWithFeatureEnabledThenIftPatcherIsNull) {
  ScopedIncrementalFontTransferForTest scoped_ift(true);
  scoped_refptr<SharedBuffer> font_buffer = LoadFont("Ahem.woff");
  base::expected<DecodedWebFont, String> decoded_result =
      DecodeWebFont(font_buffer.get());

  ASSERT_TRUE(decoded_result.has_value());
  EXPECT_NE(decoded_result->sk_typeface, nullptr);
  EXPECT_EQ(decoded_result->ift_patcher, nullptr);
  EXPECT_GT(decoded_result->decoded_size, 0u);
}

}  // namespace blink
