// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"

UIAlertController* InterruptionAlertController(ProceduralBlock cancel_block) {
  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"TODO Upload in Progress"
                       message:
                           @"TODO Your upload will be canceled if you leave "
                           @"this folder."
                preferredStyle:UIAlertControllerStyleAlert];

  void (^cancelHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    if (cancel_block) {
      cancel_block();
    }
  };

  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertAction* cancelUploadAction =
      [UIAlertAction actionWithTitle:@"TODO Cancel Upload"
                               style:UIAlertActionStyleDestructive
                             handler:cancelHandler];

  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertAction* continueUploadAction =
      [UIAlertAction actionWithTitle:@"TODO Continue Upload"
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alert addAction:cancelUploadAction];
  [alert addAction:continueUploadAction];

  return alert;
}

UIAlertController* FailAlertController(ProceduralBlock retry_block,
                                       ProceduralBlock cancel_block) {
  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"TODO This File Couldn't Be Opened"
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

  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertAction* retryAction =
      [UIAlertAction actionWithTitle:@"TODO Try Again"
                               style:UIAlertActionStyleDefault
                             handler:retryHandler];

  // TODO(crbug.com/344812548): Add a11y title.
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"TODO Cancel"
                               style:UIAlertActionStyleCancel
                             handler:cancelHandler];

  [alert addAction:retryAction];
  [alert addAction:cancelAction];

  return alert;
}
