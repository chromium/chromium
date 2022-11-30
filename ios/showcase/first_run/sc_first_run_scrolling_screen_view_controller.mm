// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_scrolling_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCFirstRunScrollingScreenViewController ()

@end

@implementation SCFirstRunScrollingScreenViewController

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Scrolling Screen";
  self.subtitleText =
      @"New FRE screen with long content to simulate dynamic types, forcing "
      @"content to scroll. Lorem ipsum dolor sit amet, consectetur adipiscing "
      @"elit. Duis volutpat auctor pretium. Donec quis turpis semper, laoreet "
      @"tellus vel, vehicula elit. Vestibulum venenatis convallis dolor eget "
      @"venenatis. Fusce in volutpat metus. Integer eget quam a orci ultrices "
      @"gravida. Sed a justo sit amet lorem scelerisque rhoncus. Aenean ac "
      @"erat ut ipsum feugiat tempus. Sed consectetur, diam ac rutrum auctor, "
      @"erat tortor semper libero, id consequat eros ligula at nulla. Donec "
      @"vel scelerisque nibh, ac laoreet magna. Sed orci lacus, auctor sit "
      @"amet nisi vel, imperdiet iaculis augue. Duis venenatis nisl sit amet "
      @"placerat euismod. In gravida lorem nec massa tincidunt. Lorem ipsum "
      @"dolor sit amet, consectetur adipiscing elit. Duis volutpat auctor "
      @"pretium. Donec quis turpis semper, laoreet tellus vel, vehicula elit. "
      @"Vestibulum venenatis convallis dolor eget venenatis. Fusce in volutpat "
      @"metus. Integer eget quam a orci ultrices gravida. Sed a justo sit amet "
      @"lorem scelerisque rhoncus.";
  self.primaryActionString = @"Continue";
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.bannerName = @"Sample-banner";
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;

  // Add some screen-specific content and its constraints.
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.text = @"Screen-specific content created by derived VC, which gets "
               @"pushed by the main content if needed.";
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
