// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

UIAlertController* FailAlertController(ProceduralBlock retry_block,
                                       ProceduralBlock cancel_block) {
  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_DRIVE_FILE_PICKER_ALERT_THIS_FILE_COUND_NOT_BE_OPENED)
                       message:nil
                preferredStyle:UIAlertControllerStyleAlert];

  void (^retryHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    if (retry_block) {
      retry_block();
    }
  };

  void (^cancelHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    if (cancel_block) {
      cancel_block();
    }
  };

  UIAlertAction* retryAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DRIVE_FILE_PICKER_ALERT_TRY_AGAIN)
                style:UIAlertActionStyleDefault
              handler:retryHandler];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:cancelHandler];

  [alert addAction:retryAction];
  [alert addAction:cancelAction];

  alert.preferredAction = retryAction;

  return alert;
}
