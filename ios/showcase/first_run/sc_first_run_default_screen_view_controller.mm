// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_default_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCFirstRunDefaultScreenViewController ()

@property(nonatomic, strong) CheckboxButton* checkboxButton;

@end

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

  self.checkboxButton = [[CheckboxButton alloc] initWithFrame:CGRectZero];
  self.checkboxButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.checkboxButton.labelText =
      @"This is a label explaining what the checkbox is for. Tap this to "
      @"toggle the checkbox!";
  self.checkboxButton.selected = YES;
  [self.checkboxButton addTarget:self
                          action:@selector(didTapCheckboxButton)
                forControlEvents:UIControlEventTouchUpInside];
  [self.specificContentView addSubview:self.checkboxButton];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [label.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [label.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],

    [self.checkboxButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:label.bottomAnchor
                                    constant:16],
    [self.checkboxButton.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.checkboxButton.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [self.checkboxButton.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

- (void)didTapCheckboxButton {
  self.checkboxButton.selected = !self.checkboxButton.selected;
}

@end
