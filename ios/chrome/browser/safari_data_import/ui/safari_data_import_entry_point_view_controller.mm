// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_entry_point_view_controller.h"

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation SafariDataImportEntryPointViewController

#pragma mark - ConfirmationAlertViewController

- (void)viewDidLoad {
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_ENTRY_POINT_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_ENTRY_POINT_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_ENTRY_POINT_PRIMARY_ACTION);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_ENTRY_POINT_SECONDARY_ACTION);
  self.image = [UIImage imageNamed:@"safari_data_import"];
  self.imageHasFixedSize = YES;
  self.topAlignedLayout = YES;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;
  [super viewDidLoad];
}

@end
