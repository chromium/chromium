// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_view_controller.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the Search What You See Lottie.
NSString* const kAnimationName = @"search_what_you_see_promo";

// The name of the animation used for the Search What You See Lottie in dark
// mode.
NSString* const kAnimationNameDarkMode = @"search_what_you_see_promo_darkmode";

// The accessibility identifier for the Search What You See view.
NSString* const kSearchWhatYouSeePromoAXID = @"kSearchWhatYouSeePromoAXID";

}  // namespace

@implementation SearchWhatYouSeePromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_SUBTITLE);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_SHOW_ME_HOW_ACTION);

  self.showDismissBarButton = YES;

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kSearchWhatYouSeePromoAXID;
}

@end
