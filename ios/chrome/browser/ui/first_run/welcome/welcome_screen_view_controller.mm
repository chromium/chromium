// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kDefaultMargin = 16;

// URL for the terms of service text.
NSString* const kTermsOfServiceUrl = @"internal://terms-of-service";

}  // namespace

@interface WelcomeScreenViewController ()

@property(nonatomic, strong) CheckboxButton* metricsConsentButton;
@property(nonatomic, strong) UILabel* termsOfServiceLabel;

@end

@implementation WelcomeScreenViewController
@dynamic delegate;

- (void)viewDidLoad {
  // TODO(crbug.com/1189815): Use final strings and enable localization.
  self.titleText = UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad
                       ? @"Test - Built for your iPad"
                       : @"Test - Built for your iPhone";
  self.subtitleText = @"Test - Get more done with a simple, secure and "
                      @"faster-than-ever Google Chrome";
  self.bannerImage = [UIImage imageNamed:@"welcome_screen_banner"];
  self.isTallBanner = YES;
  self.scrollToEndMandatory = YES;
  self.primaryActionString = @"Test - Accept and Continue";

  self.metricsConsentButton = [self createMetricsConsentButton];
  [self.specificContentView addSubview:self.metricsConsentButton];

  self.termsOfServiceLabel = [self createTermsOfServiceLabel];
  [self.specificContentView addSubview:self.termsOfServiceLabel];

  [NSLayoutConstraint activateConstraints:@[
    [self.metricsConsentButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [self.metricsConsentButton.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.metricsConsentButton.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],

    [self.termsOfServiceLabel.topAnchor
        constraintEqualToAnchor:self.metricsConsentButton.bottomAnchor
                       constant:kDefaultMargin],
    [self.termsOfServiceLabel.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.termsOfServiceLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [self.termsOfServiceLabel.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - Private

// Creates and configures the UMA consent checkbox button.
- (CheckboxButton*)createMetricsConsentButton {
  CheckboxButton* button = [[CheckboxButton alloc] initWithFrame:CGRectZero];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/1189815): Use final strings and enable localization.
  button.labelText = @"Test - Help improve Chrome by sending usage statistics "
                     @"and crash reports to Google";
  button.selected = YES;
  [button addTarget:self
                action:@selector(didTapMetricsButton)
      forControlEvents:UIControlEventTouchUpInside];

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  }

  return button;
}

// Creates and configures the label for the terms of service, with a formatted
// link to the full text of the terms of service.
- (UILabel*)createTermsOfServiceLabel {
  // TODO(crbug.com/1189815): 1) Handle taps to display the ToS text; 2) Use a
  // UITextView so only the link part is tappable.
  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1]
  };
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1],
    NSLinkAttributeName : [NSURL URLWithString:kTermsOfServiceUrl]
  };
  NSAttributedString* attributedString = AttributedStringFromStringWithLink(
      @"Test - By continuing, you agree to the BEGIN_LINKTerms of "
      @"ServiceEND_LINK",
      textAttributes, linkAttributes);

  label.attributedText = attributedString;
  return label;
}

- (void)didTapMetricsButton {
  self.metricsConsentButton.selected = !self.metricsConsentButton.selected;
}

@end
