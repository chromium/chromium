// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/scanner_alerts.h"

#include "base/logging.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a "Cancel" UIAlertAction for the given |block|.
UIAlertAction* CancelAction(scanner::CancelAlertAction block) {
  NSString* cancelButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SCANNER_ALERT_CANCEL);
  return [UIAlertAction actionWithTitle:cancelButtonTitle
                                  style:UIAlertActionStyleCancel
                                handler:block];
}

// Returns a UIAlertController with a title |title| and message |body|
// containing a single "Cancel" button with the action specified by
// |cancelBlock|.
UIAlertController* AlertWithCancelButton(
    NSString* title,
    NSString* body,
    scanner::CancelAlertAction cancelBlock) {
  UIAlertController* dialog =
      [UIAlertController alertControllerWithTitle:title
                                          message:body
                                   preferredStyle:UIAlertControllerStyleAlert];
  if (cancelBlock) {
    [dialog addAction:CancelAction(cancelBlock)];
  } else {
    [dialog addAction:CancelAction(^void(UIAlertAction*) {
              [[dialog presentingViewController]
                  dismissViewControllerAnimated:YES
                                     completion:nil];
            })];
  }
  return dialog;
}

// Returns a UIAlertController to be displayed when the camera state is
// CAMERA_PERMISSION_DENIED.
UIAlertController* CameraPermissionDeniedDialog(
    scanner::CancelAlertAction cancelBlock) {
  NSURL* settingsURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];

  if (![[UIApplication sharedApplication] canOpenURL:settingsURL]) {
    // Display a dialog instructing the user how to change the settings.
    NSString* dialogTitle =
        l10n_util::GetNSString(IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_TITLE);
    NSString* dialogBody =
        l10n_util::GetNSString(IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_DETAIL);
    return AlertWithCancelButton(dialogTitle, dialogBody, cancelBlock);
  }

  // Display a dialog with a link to the Settings app.
  NSString* dialogTitle = l10n_util::GetNSString(
      IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_TITLE_GO_TO_SETTINGS);
  NSString* dialogBody = l10n_util::GetNSString(
      IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_DETAIL_GO_TO_SETTINGS);
  NSString* settingsButton = l10n_util::GetNSString(
      IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_GO_TO_SETTINGS);

  UIAlertController* dialog =
      AlertWithCancelButton(dialogTitle, dialogBody, cancelBlock);

  void (^handler)(UIAlertAction*) = ^(UIAlertAction* action) {
    [[UIApplication sharedApplication] openURL:settingsURL
                                       options:@{}
                             completionHandler:nil];
  };

  UIAlertAction* settingsAction =
      [UIAlertAction actionWithTitle:settingsButton
                               style:UIAlertActionStyleDefault
                             handler:handler];
  [dialog addAction:settingsAction];
  [dialog setPreferredAction:settingsAction];
  return dialog;
}

}  // namespace

namespace scanner {

UIAlertController* DialogForCameraState(
    CameraState state,
    scanner::CancelAlertAction cancelBlock) {
  NSString* dialogTitle = nil;
  NSString* dialogBody = nil;
  switch (state) {
    case scanner::CAMERA_AVAILABLE:
    case scanner::CAMERA_NOT_LOADED:
      NOTREACHED();
      return nil;

    case scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION:
      dialogTitle =
          l10n_util::GetNSString(IDS_IOS_SCANNER_CAMERA_IN_USE_ALERT_TITLE);
      dialogBody =
          l10n_util::GetNSString(IDS_IOS_SCANNER_CAMERA_IN_USE_ALERT_DETAIL);
      return AlertWithCancelButton(dialogTitle, dialogBody, cancelBlock);

    case scanner::MULTIPLE_FOREGROUND_APPS:
      dialogTitle = l10n_util::GetNSString(
          IDS_IOS_SCANNER_MULTIPLE_FOREGROUND_APPS_ALERT_TITLE);
      dialogBody = l10n_util::GetNSString(
          IDS_IOS_SCANNER_MULTIPLE_FOREGROUND_APPS_ALERT_DETAIL);
      return AlertWithCancelButton(dialogTitle, dialogBody, cancelBlock);

    case scanner::CAMERA_PERMISSION_DENIED:
      return CameraPermissionDeniedDialog(cancelBlock);

    case scanner::CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE:
    case scanner::CAMERA_UNAVAILABLE:
      dialogTitle = l10n_util::GetNSString(
          IDS_IOS_SCANNER_CAMERA_UNAVAILABLE_ALERT_TITLE);
      dialogBody = l10n_util::GetNSString(
          IDS_IOS_SCANNER_CAMERA_UNAVAILABLE_ALERT_DETAIL);
      return AlertWithCancelButton(dialogTitle, dialogBody, cancelBlock);
  }
}

}  // namespace scanner
