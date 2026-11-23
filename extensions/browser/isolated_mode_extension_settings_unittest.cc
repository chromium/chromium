// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/isolated_mode_extension_settings.h"

#include <utility>

#include "base/values.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class IsolatedModeExtensionSettingsTest : public testing::Test {
 protected:
  void SetUp() override {
    ExtensionPrefs::RegisterProfilePrefs(pref_service_.registry());
    user_prefs::UserPrefs::Set(&browser_context_, &pref_service_);
  }

  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  content::TestBrowserContext browser_context_;
};

// Verifies that the setting defaults to `kAlwaysEnabled` when the preference
// is unset.
TEST_F(IsolatedModeExtensionSettingsTest, DefaultsToAlwaysEnabledWhenUnset) {
  EXPECT_EQ(GetIsolatedModeForceInstalledExtensionsSetting(&browser_context_),
            IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled);
}

// Verifies that managed policy values are respected and removing the managed
// preference falls back to `kAlwaysEnabled`.
TEST_F(IsolatedModeExtensionSettingsTest, RespectsManagedPref) {
  pref_service_.SetManagedPref(
      pref_names::kIsolatedModeForceInstalledExtensionsSettings,
      base::Value(std::to_underlying(
          IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled)));
  EXPECT_EQ(GetIsolatedModeForceInstalledExtensionsSetting(&browser_context_),
            IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled);

  pref_service_.SetManagedPref(
      pref_names::kIsolatedModeForceInstalledExtensionsSettings,
      base::Value(std::to_underlying(
          IsolatedModeForceInstalledExtensionsSetting::kEnabledByDefault)));
  EXPECT_EQ(GetIsolatedModeForceInstalledExtensionsSetting(&browser_context_),
            IsolatedModeForceInstalledExtensionsSetting::kEnabledByDefault);

  pref_service_.RemoveManagedPref(
      pref_names::kIsolatedModeForceInstalledExtensionsSettings);
  EXPECT_EQ(GetIsolatedModeForceInstalledExtensionsSetting(&browser_context_),
            IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled);
}

// Verifies that an unmanaged user preference is ignored and falls back to
// `kAlwaysEnabled`.
TEST_F(IsolatedModeExtensionSettingsTest, IgnoresUserPref) {
  pref_service_.SetUserPref(
      pref_names::kIsolatedModeForceInstalledExtensionsSettings,
      base::Value(std::to_underlying(
          IsolatedModeForceInstalledExtensionsSetting::kEnabledByDefault)));
  EXPECT_EQ(GetIsolatedModeForceInstalledExtensionsSetting(&browser_context_),
            IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled);
}

}  // namespace

}  // namespace extensions
