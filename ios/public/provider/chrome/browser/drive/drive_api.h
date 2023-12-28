// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DRIVE_DRIVE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DRIVE_DRIVE_API_H_

#import <memory>

namespace drive {
class DriveService;
struct DriveServiceConfiguration;
}  // namespace drive

namespace ios::provider {

// Creates a new instance of DriveService.
std::unique_ptr<drive::DriveService> CreateDriveService(
    const drive::DriveServiceConfiguration& configuration);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DRIVE_DRIVE_API_H_
