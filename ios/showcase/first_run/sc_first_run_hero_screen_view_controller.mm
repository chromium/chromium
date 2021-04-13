// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_hero_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

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

  // Add some screen-specific content and its constraints.
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.text = @"Screen-specific content created by derived VC";
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  [self.specificContentView addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [label.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [label.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [label.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

@end
