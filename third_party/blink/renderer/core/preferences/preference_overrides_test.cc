// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_overrides.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"

namespace blink {

TEST(PreferenceOverrides, GetOverrideInitial) {
  PreferenceOverrides overrides;

  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());
  EXPECT_FALSE(overrides.GetPreferredContrast().has_value());
  EXPECT_FALSE(overrides.GetPrefersReducedMotion().has_value());
  EXPECT_FALSE(overrides.GetPrefersReducedTransparency().has_value());
  EXPECT_FALSE(overrides.GetPrefersReducedData().has_value());
}

TEST(PreferenceOverrides, SetOverrideInvalid) {
  PreferenceOverrides overrides;

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "1px");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "orange");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "orange");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());
}

TEST(PreferenceOverrides, SetOverrideValid) {
  PreferenceOverrides overrides;

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "light");
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kLight,
            overrides.GetPreferredColorScheme());

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "dark");
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kDark,
            overrides.GetPreferredColorScheme());

  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  EXPECT_TRUE(overrides.GetPrefersReducedTransparency().value());

  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature,
      "no-preference");
  EXPECT_FALSE(overrides.GetPrefersReducedTransparency().value());
}

TEST(PreferenceOverrides, ResetOverride) {
  PreferenceOverrides overrides;

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "light");
  EXPECT_TRUE(overrides.GetPreferredColorScheme().has_value());
  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "light");
  EXPECT_TRUE(overrides.GetPreferredColorScheme().has_value());
  overrides.SetOverride(media_feature_names::kPrefersColorSchemeMediaFeature,
                        "invalid");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  EXPECT_TRUE(overrides.GetPrefersReducedTransparency().has_value());
  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "");
  EXPECT_FALSE(overrides.GetPrefersReducedTransparency().has_value());

  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  EXPECT_TRUE(overrides.GetPrefersReducedTransparency().has_value());
  overrides.SetOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "invalid");
  EXPECT_FALSE(overrides.GetPrefersReducedTransparency().has_value());
}

}  // namespace blink
