// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_view_controller.h"

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

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kSearchWhatYouSeePromoAXID;
}

@end
