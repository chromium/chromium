// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation DriveFilePickerNavigationController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kDriveFilePickerAccessibilityIdentifier;
}

@end
