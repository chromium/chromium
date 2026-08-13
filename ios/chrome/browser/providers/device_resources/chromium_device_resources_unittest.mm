// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/public/provider/chrome/browser/device_resources/device_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

constexpr CGFloat kSymbolSize = 22;

}  // namespace

using ChromiumDeviceResourcesTest = PlatformTest;
using syncer::DeviceInfo;

// Tests that GetDeviceTypeIcon returns the expected symbol images for each
// device form factor in the Chromium fallback implementation.
TEST_F(ChromiumDeviceResourcesTest,
       GetDeviceTypeIcon_ReturnsExpectedSymbolsForFormFactors) {
  // Test phone form factor returns iPhone symbol.
  UIImage* phone_image = ios::provider::GetDeviceTypeIcon(
      DeviceInfo::FormFactor::kPhone, DeviceInfo::OsType::kIOS);
  EXPECT_NSNE(nil, phone_image);
  EXPECT_NSEQ(UIImagePNGRepresentation(phone_image),
              UIImagePNGRepresentation(MakeSymbolMonochrome(
                  SymbolWithPointSize(SymbolIPhone, kSymbolSize))));

  // Test tablet form factor returns iPad symbol.
  UIImage* tablet_image = ios::provider::GetDeviceTypeIcon(
      DeviceInfo::FormFactor::kTablet, DeviceInfo::OsType::kIOS);
  EXPECT_NSNE(nil, tablet_image);
  EXPECT_NSEQ(UIImagePNGRepresentation(tablet_image),
              UIImagePNGRepresentation(MakeSymbolMonochrome(
                  SymbolWithPointSize(SymbolIPad, kSymbolSize))));

  // Test desktop form factor returns laptop symbol.
  UIImage* desktop_image = ios::provider::GetDeviceTypeIcon(
      DeviceInfo::FormFactor::kDesktop, DeviceInfo::OsType::kMac);
  EXPECT_NSNE(nil, desktop_image);
  EXPECT_NSEQ(UIImagePNGRepresentation(desktop_image),
              UIImagePNGRepresentation(MakeSymbolMonochrome(
                  SymbolWithPointSize(SymbolLaptop, kSymbolSize))));

  // Test fallback form factor returns laptop symbol.
  UIImage* unknown_image = ios::provider::GetDeviceTypeIcon(
      DeviceInfo::FormFactor::kUnknown, DeviceInfo::OsType::kUnknown);
  EXPECT_NSNE(nil, unknown_image);
  EXPECT_NSEQ(UIImagePNGRepresentation(unknown_image),
              UIImagePNGRepresentation(MakeSymbolMonochrome(
                  SymbolWithPointSize(SymbolLaptop, kSymbolSize))));
}
