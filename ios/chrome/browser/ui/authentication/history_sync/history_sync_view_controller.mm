// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation HistorySyncViewController

@dynamic delegate;

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kHistorySyncViewAccessibilityIdentifier;
  self.shouldHideBanner = YES;
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

@end
