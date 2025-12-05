// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_availability.h"

#import <Foundation/Foundation.h>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// LINT.IfChange(FilePickerDriveDisplayed)

// Whether the Drive element was displayed, and the reason why it was not.
enum class FilePickerDriveDisplayed {
  kDisplayed = 0,
  kNoWebState = 1,
  kDisabled = 2,
  kOTR = 3,
  kNotSignedIn = 4,
  kDisabledByPolicy = 5,
  kDriveServiceNotSupported = 6,
  kMaxValue = kDriveServiceNotSupported,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFilePickerDriveDisplayed)

}  // namespace

namespace drive {

bool IsSaveToDriveAvailable(bool is_incognito,
                            signin::IdentityManager* identity_manager,
                            drive::DriveService* drive_service,
                            PrefService* pref_service) {
  // Check if DriveService is supported.
  if (!drive_service || !drive_service->IsSupported()) {
    return false;
  }

  // Check policy.
  if (!pref_service ||
      pref_service->GetInteger(
          prefs::kIosSaveToDriveDownloadManagerPolicySettings) ==
          static_cast<int>(SaveToDrivePolicySettings::kDisabled)) {
    return false;
  }

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

bool IsChooseFromDriveAvailable(web::WebState* web_state,
                                bool is_incognito,
                                signin::IdentityManager* identity_manager,
                                drive::DriveService* drive_service,
                                PrefService* pref_service) {
  // Check WebState still exists.
  if (!web_state) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                  FilePickerDriveDisplayed::kNoWebState);
    return false;
  }

  // Check flag is enabled.
  if (!base::FeatureList::IsEnabled(kIOSChooseFromDrive)) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                  FilePickerDriveDisplayed::kDisabled);
    return false;
  }

  // Check WebState is not Incognito.
  if (is_incognito) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                  FilePickerDriveDisplayed::kOTR);
    return false;
  }

  // Check user is signed in.
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                  FilePickerDriveDisplayed::kNotSignedIn);
    return false;
  }

  // Check enterprise policy.
  CHECK(pref_service);
  if (pref_service->GetInteger(
          prefs::kIosChooseFromDriveFilePickerPolicySettings) ==
      static_cast<int>(ChooseFromDrivePolicySettings::kDisabled)) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                  FilePickerDriveDisplayed::kDisabledByPolicy);
    return false;
  }

  // Check if DriveService is supported.
  if (!drive_service || !drive_service->IsSupported()) {
    base::UmaHistogramEnumeration(
        "IOS.FilePicker.Drive.Displayed",
        FilePickerDriveDisplayed::kDriveServiceNotSupported);
    return false;
  }

  base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Displayed",
                                FilePickerDriveDisplayed::kDisplayed);
  return true;
}

}  // namespace drive
