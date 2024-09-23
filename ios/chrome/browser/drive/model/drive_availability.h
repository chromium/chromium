// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_AVAILABILITY_H_

class PrefService;
namespace signin {
class IdentityManager;
}

namespace drive {

class DriveService;

// Returns whether the Save to Drive entry point can be presented.
bool IsSaveToDriveAvailable(bool is_incognito,
                            signin::IdentityManager* identity_manager,
                            drive::DriveService* drive_service,
                            PrefService* pref_service);

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_AVAILABILITY_H_
