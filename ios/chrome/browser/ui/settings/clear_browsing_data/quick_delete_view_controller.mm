// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface QuickDeleteViewController () <ConfirmationAlertActionHandler>
@end

@implementation QuickDeleteViewController

#pragma mark - UIViewController

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  // TODO(crbug.com/335387869): Add header, with title and image, time range and
  // browsing data rows.

  self.titleString = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  self.primaryActionString = l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CANCEL);

  self.actionHandler = self;

  [super viewDidLoad];

  UIButtonConfiguration* buttonConfiguration =
      self.primaryActionButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kRedColor];
  self.primaryActionButton.configuration = buttonConfiguration;
}

#pragma mark - TableViewBottomSheetViewController

- (NSUInteger)rowCount {
  // TODO(crbug.com/335387869): Add rows to select the time range and the data
  // to be deleted.
  return 0;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/335387869): Trigger deletion.
}

- (void)confirmationAlertSecondaryAction {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

@end
