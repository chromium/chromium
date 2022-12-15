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

constexpr CGFloat customImageWidth = 60;
constexpr CGFloat customImageHeight = 60;
constexpr CGFloat customSpacingBeforeImageIfNoToolbar = 36;
constexpr CGFloat customSpacingAfterImage = 24;

}  // namespace

@implementation FeedSignInPromoViewController

- (void)viewDidLoad {
  self.imageHasFixedSize = YES;
  self.customSpacingBeforeImageIfNoToolbar =
      customSpacingBeforeImageIfNoToolbar;
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

  self.image = [self signInLogo];

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)updateStylingForSecondaryTitleLabel:(UILabel*)secondaryTitleLabel {
  secondaryTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

#pragma mark - Private

// Creates and configures the logo image.
- (UIImage*)signInLogo {
  UIImage* logo = [UIImage imageNamed:@"sign_in_promo_logo"];
  UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logo];
  logoImageView.frame = CGRectMake(0, 0, customImageWidth, customImageHeight);
  logoImageView.center = logoImageView.superview.center;
  logoImageView.contentMode = UIViewContentModeScaleAspectFit;
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  return logo;
}

@end
