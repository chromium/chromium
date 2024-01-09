// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_CONFIGURATION_H_

class ChromeAccountManagerService;
@protocol SingleSignOnService;

namespace drive {

// Configuration object used by the DriveService.
struct DriveServiceConfiguration {
  // The SingleSignOnService instance to use by DriveService.
  id<SingleSignOnService> sso_service;
  // The account manager service to observe system identities.
  ChromeAccountManagerService* account_manager_service;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_CONFIGURATION_H_
