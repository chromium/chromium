// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::CreateTestFont;

namespace blink {

const FontFallbackPriority FallbackPriorities[] = {
    FontFallbackPriority::kText, FontFallbackPriority::kEmojiText,
    FontFallbackPriority::kEmojiEmoji};

class TestReset : public testing::TestWithParam<FontFallbackPriority> {};

INSTANTIATE_TEST_SUITE_P(FontFallbackIteratorTest,
                         TestReset,
                         testing::ValuesIn(FallbackPriorities));

TEST_P(TestReset, TestResetWithFallbackPriority) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  const FontFallbackPriority fallback_priorities = TestReset::GetParam();
  FontDescription::VariantLigatures ligatures(
      FontDescription::kDisabledLigaturesState);
  Font test_font =
      CreateTestFont(AtomicString("TestFont"),
                     test::PlatformTestDataPath("Ahem.woff"), 100, &ligatures);

  FontFallbackIterator fallback_iterator =
      test_font.CreateFontFallbackIterator(fallback_priorities);
  FontFallbackIterator fallback_iterator_reset =
      test_font.CreateFontFallbackIterator(fallback_priorities);

  FontFallbackIterator::HintCharList fallback_chars_hint;
  fallback_iterator_reset.Next(fallback_chars_hint);
  fallback_iterator_reset.Reset();

  EXPECT_EQ(fallback_iterator_reset, fallback_iterator);
}

}  // namespace blink
