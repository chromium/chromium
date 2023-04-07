// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat customSpacingBeforeImageIfNoNavigationBar = 24;
constexpr CGFloat customSpacingAfterImage = 24;

}  // namespace

@implementation FeedSignInPromoViewController

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"sign_in_promo_logo"];
  self.imageHasFixedSize = YES;
  self.customSpacingAfterImage = customSpacingAfterImage;
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_PROMO_TITLE);
  self.secondaryTitleString =
      l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_PROMO_DESC);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_PROMO_CONTINUE_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_PROMO_CANCEL_BUTTON);

  if (@available(iOS 15, *)) {
    self.titleTextStyle = UIFontTextStyleTitle2;
    self.customSpacingBeforeImageIfNoNavigationBar =
        customSpacingBeforeImageIfNoNavigationBar;
    self.customSpacingAfterImage = 1;
    self.topAlignedLayout = YES;
  }

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle {
  secondaryTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryTitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

@end
