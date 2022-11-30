// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_feature_overrides.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"

namespace blink {

TEST(MediaFeatureOverrides, GetOverrideInitial) {
  MediaFeatureOverrides overrides;

  EXPECT_FALSE(overrides.GetColorGamut().has_value());
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());
}

TEST(MediaFeatureOverrides, SetOverrideInvalid) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "1px");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride("prefers-color-scheme", "orange");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());
}

TEST(MediaFeatureOverrides, SetOverrideValid) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "light");
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kLight,
            overrides.GetPreferredColorScheme());

  overrides.SetOverride("prefers-color-scheme", "dark");
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kDark,
            overrides.GetPreferredColorScheme());
}

TEST(MediaFeatureOverrides, ResetOverride) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "light");
  EXPECT_TRUE(overrides.GetPreferredColorScheme().has_value());
  overrides.SetOverride("prefers-color-scheme", "");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());

  overrides.SetOverride("prefers-color-scheme", "light");
  EXPECT_TRUE(overrides.GetPreferredColorScheme().has_value());
  overrides.SetOverride("prefers-color-scheme", "invalid");
  EXPECT_FALSE(overrides.GetPreferredColorScheme().has_value());
}

}  // namespace blink
