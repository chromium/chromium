// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#include <algorithm>
#include <map>

#include "components/version_info/channel.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using KioskModeInfoManifestTest = ManifestTest;

namespace {

enum class EnabledOnLaunchState { kUndefined, kEnabled, kDisabled };

std::map<std::string, EnabledOnLaunchState> GetSecondaryApps(
    const std::vector<SecondaryKioskAppInfo>& info) {
  std::map<std::string, EnabledOnLaunchState> mapped_info;
  for (const auto& app : info) {
    EXPECT_EQ(0u, mapped_info.count(app.id)) << "Duplicate entry " << app.id;
    EnabledOnLaunchState enabled_on_launch = EnabledOnLaunchState::kUndefined;
    if (app.enabled_on_launch.has_value()) {
      enabled_on_launch = app.enabled_on_launch.value()
                              ? EnabledOnLaunchState::kEnabled
                              : EnabledOnLaunchState::kDisabled;
    }
    mapped_info.emplace(app.id, enabled_on_launch);
  }
  return mapped_info;
}

}  // namespace

TEST_F(KioskModeInfoManifestTest, NoSecondaryApps) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_secondary_app_no_secondary_app.json"));
  EXPECT_FALSE(KioskModeInfo::HasSecondaryApps(extension.get()));
}

TEST_F(KioskModeInfoManifestTest, MultipleSecondaryApps) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_secondary_app_multi_apps.json"));
  EXPECT_TRUE(KioskModeInfo::HasSecondaryApps(extension.get()));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  ASSERT_TRUE(info);

  std::map<std::string, EnabledOnLaunchState> expected_secondary_apps = {
      {"ihplaomghjbeafnpnjkhppmfpnmdihgd", EnabledOnLaunchState::kUndefined},
      {"abcdabcdabcdabcdabcdabcdabcdabcd", EnabledOnLaunchState::kDisabled},
      {"fiehokkcgaojmbhfhlpiheggjhaedjoc", EnabledOnLaunchState::kEnabled}};

  EXPECT_EQ(expected_secondary_apps, GetSecondaryApps(info->secondary_apps));
}

TEST_F(KioskModeInfoManifestTest, MultipleSecondaryAppsWithRepeatedEntries) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);

  LoadAndExpectError("kiosk_secondary_app_multi_apps_repeated_entries.json",
                     manifest_errors::kInvalidKioskSecondaryAppsDuplicateApp);
}

TEST_F(KioskModeInfoManifestTest, MultipleSecondaryApps_StableChannel) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);

  LoadAndExpectError(
      "kiosk_secondary_app_multi_apps.json",
      manifest_errors::kInvalidKioskSecondaryAppsPropertyUnavailable);
}

TEST_F(KioskModeInfoManifestTest,
       RequiredPlatformVersionAndAlwaysUpdateAreOptional) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_required_platform_version_not_present.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_TRUE(info->required_platform_version.empty());
  EXPECT_FALSE(info->always_update);
}

TEST_F(KioskModeInfoManifestTest, RequiredPlatformVersion) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_required_platform_version.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_EQ("1234.0.0", info->required_platform_version);
}

TEST_F(KioskModeInfoManifestTest, RequiredPlatformVersionInvalid) {
  LoadAndExpectError("kiosk_required_platform_version_empty.json",
                     manifest_errors::kInvalidKioskRequiredPlatformVersion);
  LoadAndExpectError("kiosk_required_platform_version_invalid.json",
                     manifest_errors::kInvalidKioskRequiredPlatformVersion);
}

TEST_F(KioskModeInfoManifestTest, AlwaysUpdate) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_always_update.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_TRUE(info->always_update);
}

TEST_F(KioskModeInfoManifestTest, AlwaysUpdateFalse) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_always_update_false.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_FALSE(info->always_update);
}

TEST_F(KioskModeInfoManifestTest, AlwaysUpdateInvalid) {
  LoadAndExpectError("kiosk_always_update_invalid.json",
                     manifest_errors::kInvalidKioskAlwaysUpdate);
}

}  // namespace extensions
