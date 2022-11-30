// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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

  // TODO(crbug.com/1382615): Add string to the grd file when they are
  // finalized.
  self.titleString = @"Sign in to control what you see";
  self.secondaryTitleString = @"To personalized your Discover feed and Chrome, "
                              @"sign in and turn on sync";

  self.primaryActionString = @"Continue";
  self.secondaryActionString = @"Cancel";

  // TODO(crbug.com/1382615): Add image.

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)updateStylingForSecondaryTitleLabel:(UILabel*)secondaryTitleLabel {
  secondaryTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

@end
