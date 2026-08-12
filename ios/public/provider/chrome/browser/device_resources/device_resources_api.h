// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_RESOURCES_DEVICE_RESOURCES_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_RESOURCES_DEVICE_RESOURCES_API_H_

#import <UIKit/UIKit.h>

#import "components/sync_device_info/device_info.h"

namespace ios::provider {

// Returns the icon for a device based on its `form_factor` and `os_type`.
UIImage* GetDeviceTypeIcon(syncer::DeviceInfo::FormFactor form_factor,
                           syncer::DeviceInfo::OsType os_type);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_RESOURCES_DEVICE_RESOURCES_API_H_
