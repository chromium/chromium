// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_METRICS_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_METRICS_H_

#import <Foundation/Foundation.h>

// UMA histogram names.
extern const char kSaveToPhotosActionsHistogram[];
extern const char kSaveToPhotosAccountPickerActionsHistogram[];
extern const char kSaveToPhotosContextMenuActionsHistogram[];
extern const char kSaveToPhotosSettingsActionsHistogram[];
extern const char kSaveToPhotosUploadSuccessLatencyHistogram[];
extern const char kSaveToPhotosUploadFailureLatencyHistogram[];
extern const char kSaveToPhotosUploadFailureTypeHistogram[];

// Enum for the IOS.SaveToPhotos histogram.
// Keep in sync with "IOSSaveToPhotosType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class SaveToPhotosActions {
  kFailureWebStateDestroyed = 0,
  kFailureUserSignedOut = 1,
  kFailureUserCancelledWithTryAgainAlert =
      2,  // User cancelled using the "Cancel" option in the "This File Could
          // not be Uploaded" alert
  kFailureUserCancelledWithAccountPicker = 3,
  kFailureUserCancelledWithSnackbar = 4,  // User cancelled using the snackbar
  kSuccess = 5,
  kSuccessAndOpenPhotosApp = 6,
  kSuccessAndOpenStoreKitAndAppNotInstalled = 7,
  kSuccessAndOpenStoreKitAndAppInstalled = 8,
  kFailureOutOfStorageDidManageStorage = 9,
  kFailureOutOfStorageDidNotManageStorage = 10,
  kMaxValue = kFailureOutOfStorageDidNotManageStorage,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the IOS.SaveToPhotos.AccountPicker histogram.
// Keep in sync with "IOSSaveToPhotosAccountPickerType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class SaveToPhotosAccountPickerActions {
  kSkipped =
      0,  // Account picker not presented because a default account exists
  kCancelled = 1,         // User tapped 'Cancel' in the account picker
  kSelectedIdentity = 2,  // User selected an identity in the account picker
  kMaxValue = kSelectedIdentity,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the IOS.SaveToPhotos.ContextMenu histogram.
// Keep in sync with "IOSSaveToPhotosContextMenuType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class SaveToPhotosContextMenuActions {
  kUnavailableDidSaveImageLocally =
      0,  // "Save to Google Photos" action was unavailable and the user tapped
          // "Save to Photos" (saved image locally)
  kAvailableDidSaveImageLocally =
      2,  // "Save to Google Photos" action was available but the user tapped
          // "Save to Photos" (saved image locally)
  kAvailableDidSaveImageToGooglePhotos =
      3,  // "Save to Google Photos" action was available and the user tapped
          // "Save to Google Photos"
  kMaxValue = kAvailableDidSaveImageToGooglePhotos,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the IOS.SaveToPhotos.Settings histogram.
// Keep in sync with "IOSSaveToPhotosSettingsType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class SaveToPhotosSettingsActions {
  kDefaultAccountNotSet =
      0,  // User has NOT set a default Save to Photos account and did NOT
          // opt-in to skip the account picker
  kDefaultAccountSetAndValid =
      1,  // User has set a default Save to Photos account which exists on
          // device and did NOT opt-in to skip the account picker
  kDefaultAccountSetNotValid =
      2,  // User has set a default Save to Photos account but it is not on
          // device anymore and did NOT opt-in to skip the account picker
  kDefaultAccountSetAndValidSkipAccountPicker =
      3,  // User has set a default Save to Photos account which exists on
          // device and did opt-in to skip the account picker
  kDefaultAccountSetNotValidSkipAccountPicker =
      4,  // User has set a default Save to Photos account but it is not on
          // device anymore; the user did opt-in to skip the account picker
  kMaxValue = kDefaultAccountSetNotValidSkipAccountPicker,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Types of failure which can occur during upload.
// Keep in sync with "IOSPhotosServiceUploadFailureType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class PhotosServiceUploadFailureType : NSUInteger {
  // No failure
  kNone = 0,
  // There is already an ongoing upload.
  kAlreadyUploading = 1,
  // An error occurred while trying to retrieve an existing album.
  kRetrieveAlbum = 10,
  // An error occurred while trying to create a new album.
  kCreateAlbum = 20,
  // An error occurred during the first step of a photo upload.
  kUploadPhoto1 = 30,
  kUploadPhoto1NoData = 31,
  kUploadPhoto1ContentIsNotText = 32,
  kUploadPhoto1TokenIsNil = 33,
  // An error occurred during the second step of a photo upload.
  kUploadPhoto2 = 40,
  // Second step of a photo upload failed because the remaining storage in the
  // user's account is not enough to store the photo.
  kUploadPhoto2NotEnoughStorage = 41,
  kMaxValue = kUploadPhoto2NotEnoughStorage,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_METRICS_H_
