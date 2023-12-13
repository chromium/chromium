// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_availability.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace drive {

bool IsSaveToDriveAvailable(bool is_incognito,
                            signin::IdentityManager* identity_manager,
                            drive::DriveService* drive_service) {
  // Check flag.
  if (!base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    return false;
  }

  // Check if DriveService is supported.
  if (!drive_service || !drive_service->IsSupported()) {
    return false;
  }

  // TODO(crbug.com/1497976): Check policy.

  // Check incognito.
  if (is_incognito) {
    return false;
  }

  // Check user is signed in.
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;
}

}  // namespace drive
