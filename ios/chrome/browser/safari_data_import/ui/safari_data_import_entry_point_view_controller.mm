// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_entry_point_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/safari_data_import/public/ui_utils.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
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
  self.secondaryActionString = l10n_util::GetNSString(IDS_NO_THANKS);
  if (self.showReminderButton) {
    self.tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_ENTRY_POINT_REMINDER_ACTION);
  }
  self.image = [UIImage imageNamed:@"safari_data_import"];
  self.imageHasFixedSize = YES;
  self.topAlignedLayout = YES;
  /// Create an empty title view to show in the navigation bar if dismiss
  /// button would not be displayed.
  self.titleView = [[UIView alloc] initWithFrame:CGRectZero];
  self.showDismissBarButton = NO;
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      GetSafariDataEntryPointAccessibilityIdentifier();
  /// Hide the image on compact height.
  self.alwaysShowImage = NO;
  [self updateUIOnTraitChange];
  NSArray<UITrait>* traits = @[
    UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class,
    UITraitPreferredContentSizeCategory.class
  ];
  [self registerForTraitChanges:traits
                     withAction:@selector(updateUIOnTraitChange)];
}

#pragma mark - Accessor

- (UIFontTextStyle)titleTextStyle {
  return GetSafariDataImportTitleLabelFontTextStyle(self.traitCollection);
}

#pragma mark - Private

/// Method that updates the title font on trait collection change.
- (void)updateUIOnTraitChange {
  self.titleLabel.font = GetFRETitleFont(self.titleTextStyle);
}

@end
