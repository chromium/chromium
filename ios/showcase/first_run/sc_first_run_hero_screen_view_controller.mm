// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_hero_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCFirstRunHeroScreenViewController ()

@end

@implementation SCFirstRunHeroScreenViewController

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Hero Screen";
  self.subtitleText =
      @"New FRE screen with a large hero banner and only one primary button.";
  self.primaryActionString = @"Accept and continue";
  self.bannerImage = [UIImage imageNamed:@"Sample-banner-tall"];
  self.isTallBanner = YES;

  // TODO(crbug.com/1186762): Add screen-specific content to the shared
  // container view once it's added.

  [super viewDidLoad];
}

@end
