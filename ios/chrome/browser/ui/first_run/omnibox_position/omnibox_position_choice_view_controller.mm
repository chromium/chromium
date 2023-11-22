// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"

@implementation OmniboxPositionChoiceViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  // TODO(crbug.com/1503638): Implement this and remove placeholder text.
  self.bannerName = @"default_browser_screen_banner";
  self.titleText = @"**Tailor to Your Needs**";
  self.subtitleText = @"**Decide the position of the search bar to tailor your "
                      @"needs and browsing habits**";
  self.primaryActionString = @"**Finish**";

  [super viewDidLoad];
}

@end
