// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_path.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "ui/gfx/font_fallback_linux.h"
#endif

using blink::test::CreateTestFont;

namespace blink {

const FontFallbackPriority FallbackPriorities[] = {
    FontFallbackPriority::kText, FontFallbackPriority::kEmojiText,
    FontFallbackPriority::kEmojiEmoji};

class TestReset : public FontTestBase,
                  public testing::WithParamInterface<FontFallbackPriority> {};

INSTANTIATE_TEST_SUITE_P(FontFallbackIteratorTest,
                         TestReset,
                         testing::ValuesIn(FallbackPriorities));

TEST_P(TestReset, TestResetWithFallbackPriority) {
  const FontFallbackPriority fallback_priorities = TestReset::GetParam();
  FontDescription::VariantLigatures ligatures(
      FontDescription::kDisabledLigaturesState);
  Font* test_font =
      CreateTestFont(AtomicString("TestFont"),
                     test::PlatformTestDataPath("Ahem.woff"), 100, &ligatures);

  FontFallbackIterator fallback_iterator =
      test_font->CreateFontFallbackIterator(fallback_priorities);
  FontFallbackIterator fallback_iterator_reset =
      test_font->CreateFontFallbackIterator(fallback_priorities);

  FontFallbackIterator::HintCharList fallback_chars_hint{0x1F600};
  while (fallback_iterator_reset.HasNext()) {
    fallback_iterator_reset.Next(fallback_chars_hint);
  }
  fallback_iterator_reset.Reset();

  EXPECT_EQ(fallback_iterator_reset, fallback_iterator);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
namespace {

class FakeSandboxSupport final : public WebSandboxSupport {
 public:
  FakeSandboxSupport(Vector<String> font_paths, bool* armed)
      : font_paths_(std::move(font_paths)), armed_(armed) {}

  bool GetFallbackFontForCharacter(
      WebUChar32 character,
      const char* preferred_locale,
      gfx::FallbackFontData* fallback_font) override {
    // Disarm sandbox support after recording the fallback query so
    // `FontCache::CreateTypeface()` loads `filepath` directly from disk rather
    // than through `SkFontConfigInterface`.
    *armed_ = false;
    requested_locales_.push_back(String::FromUtf8(
        std::string_view(preferred_locale ? preferred_locale : "")));
    if (next_font_index_ >= font_paths_.size()) {
      return false;
    }
    String path = font_paths_[next_font_index_++];
    fallback_font->name = path.Utf8();
    fallback_font->filepath = base::FilePath(path.Utf8());
    return true;
  }

  bool MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      gfx::FallbackFontData*) override {
    return false;
  }

  void GetWebFontRenderStyleForStrike(const char* family,
                                      int size,
                                      bool is_bold,
                                      bool is_italic,
                                      float device_scale_factor,
                                      WebFontRenderStyle*) override {}

  const Vector<String>& RequestedLocales() const { return requested_locales_; }

 private:
  Vector<String> font_paths_;
  wtf_size_t next_font_index_ = 0;
  Vector<String> requested_locales_;
  raw_ptr<bool> armed_;
};

class FallbackRecordingPlatformSupport final : public TestingPlatformSupport {
 public:
  FallbackRecordingPlatformSupport()
      : sandbox_({test::BlinkWebTestsDir() +
                      "/third_party/NotoEmoji/NotoEmoji-Regular.subset.ttf",
                  test::BlinkWebTestsDir() +
                      "/third_party/NotoColorEmoji/NotoColorEmoji.ttf",
                  test::PlatformTestDataPath(
                      "third_party/MEgalopolis/MEgalopolisExtra.woff")},
                 &armed_) {}

  ~FallbackRecordingPlatformSupport() override {
    FontCache::Get().Invalidate();
  }

  WebSandboxSupport* GetSandboxSupport() override {
    return armed_ ? &sandbox_ : nullptr;
  }

  FontDataForRangeSet* NextWithArmedSandbox(
      FontFallbackIterator& iterator,
      const FontFallbackIterator::HintCharList& hint_list) {
    armed_ = true;
    FontDataForRangeSet* range_set = iterator.Next(hint_list);
    armed_ = false;
    return range_set;
  }

  const Vector<String>& RequestedLocales() const {
    return sandbox_.RequestedLocales();
  }

 private:
  bool armed_ = false;
  FakeSandboxSupport sandbox_;
};

class FontFallbackIteratorPriorityTest : public FontTestBase {};

}  // namespace

// Regression test for https://crbug.com/579721442: when an emoji segment
// contains characters covered by distinct `und-Zsye` fonts (e.g. 😀 in
// NotoColorEmoji.ttf followed by 🇧🇷 in NotoColorEmojiFlags.ttf on Android),
// `FontFallbackIterator` must try priority fallback for each distinct unshaped
// hint character before dropping down to regular system fallback.
TEST_F(FontFallbackIteratorPriorityTest, MultipleFallbackPriorityFontsEnabled) {
  ScopedFontFallbackMultiplePriorityFontsForTest scoped_feature(true);
  ScopedTestingPlatformSupport<FallbackRecordingPlatformSupport> platform;

  Font* test_font = CreateTestFont(AtomicString("TestFont"),
                                   test::PlatformTestDataPath("Ahem.woff"), 16);
  FontFallbackIterator iterator =
      test_font->CreateFontFallbackIterator(FontFallbackPriority::kEmojiEmoji);

  // 1. Primary CSS font (`Ahem.woff`).
  FontDataForRangeSet* primary = platform->NextWithArmedSandbox(iterator, {});
  ASSERT_TRUE(primary->HasFontData());
  EXPECT_TRUE(platform->RequestedLocales().empty());

  // 2. First emoji hint (U+1F600 😀) requests color-emoji locale `und-Zsye`.
  FontDataForRangeSet* priority_font_1 =
      platform->NextWithArmedSandbox(iterator, {0x1F600, 0x1F1E7, 0x1F1F7});
  ASSERT_TRUE(priority_font_1->HasFontData());
  ASSERT_EQ(1u, platform->RequestedLocales().size());
  EXPECT_EQ("und-Zsye", platform->RequestedLocales()[0]);

  // 3. Remaining unshaped flag hint (U+1F1E7 🇧) also requests `und-Zsye`
  // rather than dropping `kEmojiEmoji` priority.
  FontDataForRangeSet* priority_font_2 =
      platform->NextWithArmedSandbox(iterator, {0x1F1E7, 0x1F1F7});
  ASSERT_TRUE(priority_font_2->HasFontData());
  EXPECT_NE(priority_font_1->FontData(), priority_font_2->FontData());
  ASSERT_EQ(2u, platform->RequestedLocales().size());
  EXPECT_EQ("und-Zsye", platform->RequestedLocales()[1]);

  // 4. If the priority font for U+1F1E7 still fails to shape it, the next
  // iteration for the same hint falls through to regular system fallback.
  FontDataForRangeSet* system_font =
      platform->NextWithArmedSandbox(iterator, {0x1F1E7, 0x1F1F7});
  ASSERT_TRUE(system_font->HasFontData());
  ASSERT_EQ(3u, platform->RequestedLocales().size());
  EXPECT_NE("und-Zsye", platform->RequestedLocales()[2]);
}

TEST_F(FontFallbackIteratorPriorityTest,
       MultipleFallbackPriorityFontsDisabled) {
  ScopedFontFallbackMultiplePriorityFontsForTest scoped_feature(false);
  ScopedTestingPlatformSupport<FallbackRecordingPlatformSupport> platform;

  Font* test_font = CreateTestFont(AtomicString("TestFont"),
                                   test::PlatformTestDataPath("Ahem.woff"), 16);
  FontFallbackIterator iterator =
      test_font->CreateFontFallbackIterator(FontFallbackPriority::kEmojiEmoji);

  ASSERT_TRUE(platform->NextWithArmedSandbox(iterator, {})->HasFontData());
  ASSERT_TRUE(
      platform->NextWithArmedSandbox(iterator, {0x1F600, 0x1F1E7, 0x1F1F7})
          ->HasFontData());
  ASSERT_EQ(1u, platform->RequestedLocales().size());
  EXPECT_EQ("und-Zsye", platform->RequestedLocales()[0]);

  // With the kill switch disabled, the second fallback lookup immediately
  // drops `und-Zsye` priority.
  ASSERT_TRUE(platform->NextWithArmedSandbox(iterator, {0x1F1E7, 0x1F1F7})
                  ->HasFontData());
  ASSERT_EQ(2u, platform->RequestedLocales().size());
  EXPECT_NE("und-Zsye", platform->RequestedLocales()[1]);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace blink
