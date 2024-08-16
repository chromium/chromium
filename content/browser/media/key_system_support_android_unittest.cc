// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_android.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/public/common/cdm_info.h"
#include "media/base/cdm_capability.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const char kWidevineKeySystem[] = "com.widevine.alpha";
const char kUnsupportedKeySystem[] = "keysystem.test.unsupported";
}  // namespace

// These tests disable the mojo remote process as it uses a sandbox which does
// not appear to be available during testing.

TEST(KeySystemSupportAndroidTest, SoftwareSecureWidevine) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kAllowMediaCodecCallsInSeparateProcess);

  base::test::TestFuture<std::optional<media::CdmCapability>> capability;
  GetAndroidCdmCapability(kWidevineKeySystem,
                          CdmInfo::Robustness::kSoftwareSecure,
                          capability.GetCallback());

  // As the capabilities depend on the device this is running on, just check
  // that we get something back. All Android devices should support some
  // form of Widevine for software secure operation.
  ASSERT_TRUE(capability.Get().has_value());
}

TEST(KeySystemSupportAndroidTest, HardwareSecureWidevine) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kAllowMediaCodecCallsInSeparateProcess);

  base::test::TestFuture<std::optional<media::CdmCapability>> capability;
  GetAndroidCdmCapability(kWidevineKeySystem,
                          CdmInfo::Robustness::kHardwareSecure,
                          capability.GetCallback());

  // As the capabilities depend on the device this is running on, just check
  // that we get something back. Some devices (e.g. emulators) will not have
  // any hardware secure capabilities.
  ASSERT_TRUE(capability.Wait());
}

TEST(KeySystemSupportAndroidTest, UnknownKeySystem) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kAllowMediaCodecCallsInSeparateProcess);

  base::test::TestFuture<std::optional<media::CdmCapability>> capability;
  GetAndroidCdmCapability(kUnsupportedKeySystem,
                          CdmInfo::Robustness::kSoftwareSecure,
                          capability.GetCallback());

  // Keysystem should not exist, so no capabilities should be found.
  ASSERT_FALSE(capability.Get().has_value());
}

}  // namespace content
