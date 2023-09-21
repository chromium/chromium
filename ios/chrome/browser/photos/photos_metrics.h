// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_PHOTOS_METRICS_H_
#define IOS_CHROME_BROWSER_PHOTOS_PHOTOS_METRICS_H_

// UMA histogram names.
extern const char kSaveToPhotosActionsHistogram[];
extern const char kSaveToPhotosAccountPickerActionsHistogram[];
extern const char kSaveToPhotosSettingsActionsHistogram[];

// Enum for the IOS.SaveToPhotos histogram.
// Keep in sync with "IOSSaveToPhotosType"
// in src/tools/metrics/histograms/enums.xml.
enum class SaveToPhotosActions {
  kFailureWebStateDestroyed = 0,
  kFailureUserSignedOut = 1,
  kFailureUserCancelledWithAlert =
      2,  // User cancelled using the "Cancel" option in the "This File Could
          // not be Uploaded" alert
  kFailureUserCancelledWithAccountPicker = 3,
  kFailureUserCancelledWithSnackbar = 4,  // User cancelled using the snackbar
  kSuccess = 5,
  kSuccessAndOpenPhotosApp = 6,
  kSuccessAndOpenStoreKitAndAppNotInstalled = 7,
  kSuccessAndOpenStoreKitAndAppInstalled = 8,
  kMaxValue = kSuccessAndOpenStoreKitAndAppInstalled,
};

// Enum for the IOS.SaveToPhotos.AccountPicker histogram.
// Keep in sync with "IOSSaveToPhotosAccountPickerType"
// in src/tools/metrics/histograms/enums.xml.
enum class SaveToPhotosAccountPickerActions {
  kSkipped =
      0,  // Account picker not presented because a default account exists
  kCancelled = 1,         // User tapped 'Cancel' in the account picker
  kSelectedIdentity = 2,  // User selected an identity in the account picker
  kMaxValue = kSelectedIdentity,
};

// Enum for the IOS.SaveToPhotos.Settings histogram.
// Keep in sync with "IOSSaveToPhotosSettingsType"
// in src/tools/metrics/histograms/enums.xml.
enum class SaveToPhotosSettingsActions {
  kDefaultAccountNotSet =
      0,  // User has NOT set a default Save to Photos account
  kDefaultAccountSetAndValid = 1,  // User has set a default Save to Photos
                                   // account which exists on device
  kDefaultAccountSetNotValid = 2,  // User has set a default Save to Photos
                                   // account but it is not on device anymore
  kMaxValue = kDefaultAccountSetNotValid,
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_PHOTOS_METRICS_H_
