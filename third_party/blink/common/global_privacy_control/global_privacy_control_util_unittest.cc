// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/global_privacy_control/global_privacy_control_util.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace blink {

class GlobalPrivacyControlUtilTest : public testing::Test {
 private:
  base::test::ScopedFeatureList feature_list_{
      /*enabled_features=*/{features::kGlobalPrivacyControlApi},
      /*disabled_features=*/{features::kGlobalPrivacyControlForce}};
};

TEST_F(GlobalPrivacyControlUtilTest, UsesEmbedderSettingWithoutOverride) {
  RendererPreferences prefs;
  ASSERT_FALSE(prefs.global_privacy_control_devtools_override.has_value());

  prefs.is_global_privacy_control_setting_enabled = false;
  EXPECT_FALSE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));

  prefs.is_global_privacy_control_setting_enabled = true;
  EXPECT_TRUE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));
}

TEST_F(GlobalPrivacyControlUtilTest, DevToolsOverrideTakesPrecedence) {
  RendererPreferences prefs;

  prefs.is_global_privacy_control_setting_enabled = true;
  prefs.global_privacy_control_devtools_override = false;
  EXPECT_FALSE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));

  prefs.is_global_privacy_control_setting_enabled = false;
  prefs.global_privacy_control_devtools_override = true;
  EXPECT_TRUE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));
}

// Regression test: clearing the DevTools override must restore the embedder's
// setting rather than leaving the last override value in effect.
TEST_F(GlobalPrivacyControlUtilTest, ClearingOverrideRestoresEmbedderSetting) {
  RendererPreferences prefs;
  prefs.is_global_privacy_control_setting_enabled = true;

  prefs.global_privacy_control_devtools_override = false;
  EXPECT_FALSE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));

  prefs.global_privacy_control_devtools_override.reset();
  EXPECT_TRUE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));
}

TEST_F(GlobalPrivacyControlUtilTest, FeatureDisabledIgnoresSettingAndOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kGlobalPrivacyControlApi,
                             features::kGlobalPrivacyControlForce});

  RendererPreferences prefs;
  prefs.is_global_privacy_control_setting_enabled = true;
  prefs.global_privacy_control_devtools_override = true;
  EXPECT_FALSE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));
}

TEST_F(GlobalPrivacyControlUtilTest, ForceFeatureOverridesEverything) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlobalPrivacyControlForce);

  RendererPreferences prefs;
  prefs.is_global_privacy_control_setting_enabled = false;
  prefs.global_privacy_control_devtools_override = false;
  EXPECT_TRUE(IsGlobalPrivacyControlFeatureAndSettingEnabled(prefs));
}

}  // namespace blink
