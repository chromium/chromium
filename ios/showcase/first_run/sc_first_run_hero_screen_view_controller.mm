// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_hero_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SCFirstRunHeroScreenViewController ()

@end

@implementation SCFirstRunHeroScreenViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Hero Screen";
  self.subtitleText =
      @"New FRE screen with a large hero banner and a primary button. Also "
      @"shows how to define custom buttons in the derived view controllers, "
      @"and how to dynamically change the primary button label.";
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.bannerName = @"Sample-banner-tall";
  self.bannerSize = BannerImageSizeType::kTall;
  self.scrollToEndMandatory = YES;

  // Add some screen-specific content and its constraints.
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.text = @"The following button is created by the derived VC and toggles "
               @"the primary button's text.";
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  [self.specificContentView addSubview:label];

  UIButton* button = [self createButton];
  [self.specificContentView addSubview:button];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [label.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [label.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],

    [button.topAnchor constraintEqualToAnchor:label.bottomAnchor],
    [button.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [button.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [button.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

- (UIButton*)createButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  NSString* buttonTitle = @"Custom button";

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  buttonConfiguration.background.backgroundColor = [UIColor clearColor];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  buttonConfiguration.baseForegroundColor = titleColor;

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:buttonTitle];
  [string addAttributes:attributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;
  button.configuration = buttonConfiguration;

  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(didTapCustomActionButton)
      forControlEvents:UIControlEventTouchUpInside];

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

- (void)didTapCustomActionButton {
  [self.delegate didTapCustomActionButton];
}

@end
