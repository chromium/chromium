// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_feature_overrides.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MediaFeatureOverrides, GetOverrideInitial) {
  MediaFeatureOverrides overrides;

  EXPECT_FALSE(overrides.GetOverride("unknown").IsValid());
  EXPECT_FALSE(overrides.GetOverride("prefers-color-scheme").IsValid());
  EXPECT_FALSE(overrides.GetOverride("display-mode").IsValid());
}

TEST(MediaFeatureOverrides, SetOverrideInvalid) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "1px");
  EXPECT_FALSE(overrides.GetOverride("prefers-color-scheme").IsValid());

  overrides.SetOverride("prefers-color-scheme", "orange");
  EXPECT_FALSE(overrides.GetOverride("prefers-color-scheme").IsValid());
}

TEST(MediaFeatureOverrides, SetOverrideValid) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "light");
  auto light_override = overrides.GetOverride("prefers-color-scheme");
  EXPECT_TRUE(light_override.IsValid());
  EXPECT_TRUE(light_override.is_id);
  EXPECT_EQ(CSSValueID::kLight, light_override.id);

  overrides.SetOverride("prefers-color-scheme", "dark");
  auto dark_override = overrides.GetOverride("prefers-color-scheme");
  EXPECT_TRUE(dark_override.IsValid());
  EXPECT_TRUE(dark_override.is_id);
  EXPECT_EQ(CSSValueID::kDark, dark_override.id);
}

TEST(MediaFeatureOverrides, ResetOverride) {
  MediaFeatureOverrides overrides;

  overrides.SetOverride("prefers-color-scheme", "light");
  EXPECT_TRUE(overrides.GetOverride("prefers-color-scheme").IsValid());
  overrides.SetOverride("prefers-color-scheme", "");
  EXPECT_FALSE(overrides.GetOverride("prefers-color-scheme").IsValid());

  overrides.SetOverride("prefers-color-scheme", "light");
  EXPECT_TRUE(overrides.GetOverride("prefers-color-scheme").IsValid());
  overrides.SetOverride("prefers-color-scheme", "invalid");
  EXPECT_FALSE(overrides.GetOverride("prefers-color-scheme").IsValid());
}

}  // namespace blink
