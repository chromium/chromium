// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation PlusAddressBottomSheetViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_OK_TEXT);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT);
  [super viewDidLoad];
}

@end
