// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_os_info_override_win.h"
#include "services/device/public/mojom/geolocation_internals.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace features {
namespace {

using ::device::mojom::LocationProviderManagerMode;

// The Windows.Security.Authorization.AppCapabilityAccess namespace is only
// available starting with Windows 10, version 1903 (build 18362), so the
// system-level location permission integration must stay disabled on older
// releases.
TEST(DeviceFeaturesWinTest, OsLevelGeolocationPermissionSupportedOnWin10) {
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1);

  EXPECT_TRUE(IsOsLevelGeolocationPermissionSupportEnabled());
}

TEST(DeviceFeaturesWinTest,
     OsLevelGeolocationPermissionUnsupportedOnWinServer2019) {
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWinServer2019);

  EXPECT_FALSE(IsOsLevelGeolocationPermissionSupportEnabled());
}

// `LocationProviderWinrt` needs the Windows Location Platform, which is absent
// before Windows 10, version 1903. Since `kPlatformOnly` has no fallback, those
// releases must use the network provider instead.
TEST(DeviceFeaturesWinTest, LocationProviderManagerModePlatformOnlyOnWin10) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLocationProviderManager,
      {{kLocationProviderManagerParam.name, "PlatformOnly"}});
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1);

  EXPECT_EQ(GetLocationProviderManagerMode(),
            LocationProviderManagerMode::kPlatformOnly);
}

TEST(DeviceFeaturesWinTest,
     LocationProviderManagerModeForcedToNetworkOnWinServer2019) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLocationProviderManager,
      {{kLocationProviderManagerParam.name, "PlatformOnly"}});
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWinServer2019);

  EXPECT_EQ(GetLocationProviderManagerMode(),
            LocationProviderManagerMode::kNetworkOnly);
}

// Windows defaults to `kPlatformOnly`, so a disabled feature must still be
// overridden on releases that cannot support the platform provider.
TEST(DeviceFeaturesWinTest,
     LocationProviderManagerModeForcedToNetworkWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kLocationProviderManager);
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWinServer2019);

  EXPECT_EQ(GetLocationProviderManagerMode(),
            LocationProviderManagerMode::kNetworkOnly);
}

}  // namespace
}  // namespace features
