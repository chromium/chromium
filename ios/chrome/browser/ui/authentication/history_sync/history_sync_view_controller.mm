// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"

#import "base/feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller_audience.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation HistorySyncViewController

@dynamic delegate;

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kHistorySyncViewAccessibilityIdentifier;
  self.shouldHideBanner = YES;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.headerImageType = PromoStyleImageType::kAvatar;
  self.headerBackgroundImage =
      [UIImage imageNamed:@"history_sync_opt_in_background"];
  self.titleText = l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_SECONDARY_ACTION);
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (base::FeatureList::IsEnabled(
          switches::kMinorModeRestrictionsForHistorySyncOptIn)) {
    // Hide the buttons only if button visibility has not been updated.
    if (self.actionButtonsVisibility == ActionButtonsVisibility::kDefault) {
      self.actionButtonsVisibility = ActionButtonsVisibility::kHidden;
      [self.audience viewAppearedWithHiddenButtons];
    }
  }
}

#pragma mark - HistorySyncConsumer

- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage {
  self.headerImage = primaryIdentityAvatarImage;
}

- (void)setPrimaryIdentityAvatarAccessibilityLabel:
    (NSString*)primaryIdentityAvatarAccessibilityLabel {
  self.headerAccessibilityLabel = primaryIdentityAvatarAccessibilityLabel;
}

- (void)setFooterText:(NSString*)text {
  self.disclaimerText = text;
}

- (void)displayButtonsWithRestrictionStatus:(BOOL)isRestricted {
  if (base::FeatureList::IsEnabled(
          switches::kMinorModeRestrictionsForHistorySyncOptIn)) {
    self.actionButtonsVisibility =
        isRestricted ? ActionButtonsVisibility::kEquallyWeightedButtonShown
                     : ActionButtonsVisibility::kRegularButtonsShown;
  }
}

@end
