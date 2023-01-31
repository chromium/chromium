// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_default_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCFirstRunDefaultScreenViewController

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Default Screen";
  self.subtitleText = @"New FRE screen with a standard banner, a primary "
                      @"button and two secondary buttons.";
  self.primaryActionString = @"Accept and continue";
  self.secondaryActionString = @"Not now";
  self.tertiaryActionString = @"Customize sync";
  self.bannerName = @"Sample-banner";
  self.isTallBanner = NO;

  // Add some screen-specific content and its constraints.
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.text = @"Screen-specific content created by derived VC.";
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  [self.specificContentView addSubview:label];
  [super viewDidLoad];
}

@end
