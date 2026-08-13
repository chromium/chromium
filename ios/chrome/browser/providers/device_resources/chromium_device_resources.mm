// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/public/provider/chrome/browser/device_resources/device_resources_api.h"

namespace {

constexpr CGFloat kSymbolSize = 22;

}  // namespace

namespace ios::provider {

UIImage* GetDeviceTypeIcon(syncer::DeviceInfo::FormFactor form_factor,
                           syncer::DeviceInfo::OsType os_type) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kTablet:
      return MakeSymbolMonochrome(SymbolWithPointSize(SymbolIPad, kSymbolSize));
    case syncer::DeviceInfo::FormFactor::kPhone:
      return MakeSymbolMonochrome(
          SymbolWithPointSize(SymbolIPhone, kSymbolSize));
    case syncer::DeviceInfo::FormFactor::kDesktop:
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
      return MakeSymbolMonochrome(
          SymbolWithPointSize(SymbolLaptop, kSymbolSize));
  }
}

}  // namespace ios::provider
