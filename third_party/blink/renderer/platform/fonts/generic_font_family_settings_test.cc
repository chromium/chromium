// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

struct FamilyNames {
  GenericFontFamily family;
  const char* common;
  const char* arabic;
};

constexpr FamilyNames kFamilyNames[] = {
    {GenericFontFamily::kStandard, "CustomStandard", "CustomArabicStandard"},
    {GenericFontFamily::kFixed, "CustomFixed", "CustomArabicFixed"},
    {GenericFontFamily::kSerif, "CustomSerif", "CustomArabicSerif"},
    {GenericFontFamily::kSansSerif, "CustomSansSerif", "CustomArabicSansSerif"},
    {GenericFontFamily::kCursive, "CustomCursive", "CustomArabicCursive"},
    {GenericFontFamily::kFantasy, "CustomFantasy", "CustomArabicFantasy"},
    {GenericFontFamily::kMath, "CustomMath", "CustomArabicMath"},
};

void ExpectAllEmpty(const GenericFontFamilySettings& settings) {
  for (const FamilyNames& names : kFamilyNames) {
    EXPECT_TRUE(settings.Get(names.family).empty());
    EXPECT_TRUE(settings.Get(names.family, USCRIPT_ARABIC).empty());
  }
}

void UpdateAll(GenericFontFamilySettings& settings) {
  for (const FamilyNames& names : kFamilyNames) {
    settings.Update(names.family, AtomicString(names.common));
    settings.Update(names.family, AtomicString(names.arabic), USCRIPT_ARABIC);
  }
}

void ExpectAllUpdated(const GenericFontFamilySettings& settings) {
  for (const FamilyNames& names : kFamilyNames) {
    EXPECT_EQ(names.common, settings.Get(names.family));
    EXPECT_EQ(names.arabic, settings.Get(names.family, USCRIPT_ARABIC));
  }
}

}  // namespace

TEST(GenericFontFamilySettingsTest, FirstAvailableFontFamily) {
  GenericFontFamilySettings settings;
  EXPECT_TRUE(settings.Get(GenericFontFamily::kStandard).empty());

  // Returns the first available font if starts with ",".
  settings.Update(GenericFontFamily::kStandard,
                  AtomicString(",not exist, Arial"));
  EXPECT_EQ("Arial", settings.Get(GenericFontFamily::kStandard));

  // Otherwise returns any strings as they were set.
  AtomicString non_lists[] = {
      AtomicString("Arial"),
      AtomicString("not exist"),
      AtomicString("not exist, Arial"),
  };
  for (const AtomicString& name : non_lists) {
    settings.Update(GenericFontFamily::kStandard, name);
    EXPECT_EQ(name, settings.Get(GenericFontFamily::kStandard));
  }
}

TEST(GenericFontFamilySettingsTest, TestAllNames) {
  GenericFontFamilySettings settings;
  ExpectAllEmpty(settings);

  UpdateAll(settings);
  ExpectAllUpdated(settings);

  settings.Reset();
  ExpectAllEmpty(settings);
}

TEST(GenericFontFamilySettingsTest, TestSettingNotChanged) {
  GenericFontFamilySettings settings;
  ExpectAllEmpty(settings);

  // Set the initial value.
  UpdateAll(settings);
  ExpectAllUpdated(settings);

  // Set the same value again.
  for (const FamilyNames& names : kFamilyNames) {
    EXPECT_FALSE(settings.Update(names.family, AtomicString(names.common)));
    EXPECT_FALSE(settings.Update(names.family, AtomicString(names.arabic),
                                 USCRIPT_ARABIC));
  }
}

TEST(GenericFontFamilySettingsTest, TestFontListNotChanged) {
  GenericFontFamilySettings settings;

  EXPECT_TRUE(settings.Get(GenericFontFamily::kStandard).empty());

  // Returns the first available font if starts with ",".
  settings.Update(GenericFontFamily::kStandard,
                  AtomicString(",not exist, Arial"));
  EXPECT_EQ("Arial", settings.Get(GenericFontFamily::kStandard));
  // Setting to the same value will result in no change.
  EXPECT_FALSE(settings.Update(GenericFontFamily::kStandard,
                               AtomicString(",not exist, Arial")));
  EXPECT_EQ("Arial", settings.Get(GenericFontFamily::kStandard));
  EXPECT_TRUE(settings.Update(GenericFontFamily::kFixed,
                              AtomicString(",not exist, Arial")));
}

TEST(GenericFontFamilySettingsTest, TestScriptFontListNotChangedBeforeQuery) {
  GenericFontFamilySettings settings;

  EXPECT_TRUE(settings.Update(GenericFontFamily::kStandard,
                              AtomicString(",not exist, Arial"),
                              USCRIPT_KATAKANA_OR_HIRAGANA));
  // Repeatedly setting the same unresolved comma-separated font list before
  // the script is ever queried should report no change without resolving it.
  EXPECT_FALSE(settings.Update(GenericFontFamily::kStandard,
                               AtomicString(",not exist, Arial"),
                               USCRIPT_KATAKANA_OR_HIRAGANA));
  EXPECT_EQ("Arial", settings.Get(GenericFontFamily::kStandard,
                                  USCRIPT_KATAKANA_OR_HIRAGANA));
  EXPECT_FALSE(settings.Update(GenericFontFamily::kStandard,
                               AtomicString(",not exist, Arial"),
                               USCRIPT_KATAKANA_OR_HIRAGANA));

  // Updating an unresolved list whose first-available font is already cached to
  // another cached list that resolves to the same font should also report no
  // change.
  EXPECT_TRUE(settings.Update(GenericFontFamily::kStandard,
                              AtomicString(",also not exist, Arial"),
                              USCRIPT_COMMON));
  EXPECT_EQ("Arial",
            settings.Get(GenericFontFamily::kStandard, USCRIPT_COMMON));
  EXPECT_TRUE(settings.Update(GenericFontFamily::kStandard,
                              AtomicString(",not exist, Arial"),
                              USCRIPT_SIMPLIFIED_HAN));
  EXPECT_FALSE(settings.Update(GenericFontFamily::kStandard,
                               AtomicString(",also not exist, Arial"),
                               USCRIPT_SIMPLIFIED_HAN));

  // Also verify kFixed with "Osaka" (which returns "Osaka-Mono" on macOS).
  AtomicString fixed_list(",Osaka, not exist");
  EXPECT_TRUE(settings.Update(GenericFontFamily::kFixed, fixed_list,
                              USCRIPT_KATAKANA_OR_HIRAGANA));
  EXPECT_FALSE(settings.Update(GenericFontFamily::kFixed, fixed_list,
                               USCRIPT_KATAKANA_OR_HIRAGANA));
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ("Osaka-Mono", settings.Get(GenericFontFamily::kFixed,
                                       USCRIPT_KATAKANA_OR_HIRAGANA));
#else
  EXPECT_EQ("Osaka", settings.Get(GenericFontFamily::kFixed,
                                  USCRIPT_KATAKANA_OR_HIRAGANA));
#endif
  EXPECT_FALSE(settings.Update(GenericFontFamily::kFixed, fixed_list,
                               USCRIPT_KATAKANA_OR_HIRAGANA));
}

TEST(GenericFontFamilySettingsTest, ArabicNastaliqFallbackToArabic) {
  GenericFontFamilySettings settings;
  settings.Update(GenericFontFamily::kStandard, AtomicString("CustomStandard"));
  settings.Update(GenericFontFamily::kStandard,
                  AtomicString("CustomArabicStandard"), USCRIPT_ARABIC);

  {
    ScopedNastaliqScriptForTest scoped_feature(true);
    // When USCRIPT_ARABIC_NASTALIQ is not set, it falls back to USCRIPT_ARABIC.
    EXPECT_EQ("CustomArabicStandard", settings.Get(GenericFontFamily::kStandard,
                                                   USCRIPT_ARABIC_NASTALIQ));

    // When USCRIPT_ARABIC_NASTALIQ is explicitly set, it takes precedence.
    settings.Update(GenericFontFamily::kStandard,
                    AtomicString("CustomNastaliqStandard"),
                    USCRIPT_ARABIC_NASTALIQ);
    EXPECT_EQ(
        "CustomNastaliqStandard",
        settings.Get(GenericFontFamily::kStandard, USCRIPT_ARABIC_NASTALIQ));
    EXPECT_EQ("CustomArabicStandard",
              settings.Get(GenericFontFamily::kStandard, USCRIPT_ARABIC));

    // Clearing USCRIPT_ARABIC_NASTALIQ falls back to USCRIPT_ARABIC again.
    settings.Update(GenericFontFamily::kStandard, g_empty_atom,
                    USCRIPT_ARABIC_NASTALIQ);
    EXPECT_EQ("CustomArabicStandard", settings.Get(GenericFontFamily::kStandard,
                                                   USCRIPT_ARABIC_NASTALIQ));
  }

  // When NastaliqScript is disabled, USCRIPT_ARABIC_NASTALIQ falls back
  // directly to USCRIPT_COMMON.
  ScopedNastaliqScriptForTest scoped_feature(false);
  EXPECT_EQ("CustomStandard", settings.Get(GenericFontFamily::kStandard,
                                           USCRIPT_ARABIC_NASTALIQ));
}

}  // namespace blink
