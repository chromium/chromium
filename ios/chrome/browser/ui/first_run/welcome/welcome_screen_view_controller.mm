// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kDefaultMargin = 16;

// URL for the terms of service text.
NSString* const kTermsOfServiceUrl = @"internal://terms-of-service";

}  // namespace

@interface WelcomeScreenViewController () <UITextViewDelegate>

@property(nonatomic, strong) CheckboxButton* metricsConsentButton;
@property(nonatomic, strong) UITextView* termsOfServiceTextView;

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

  self.termsOfServiceTextView = [self createTermsOfServiceTextView];
  [self.specificContentView addSubview:self.termsOfServiceTextView];

  [NSLayoutConstraint activateConstraints:@[
    [self.metricsConsentButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [self.metricsConsentButton.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.metricsConsentButton.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],

    [self.termsOfServiceTextView.topAnchor
        constraintEqualToAnchor:self.metricsConsentButton.bottomAnchor
                       constant:kDefaultMargin],
    [self.termsOfServiceTextView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.termsOfServiceTextView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [self.termsOfServiceTextView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - Accessors

- (BOOL)checkBoxSelected {
  return self.metricsConsentButton.selected;
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

// Creates and configures the text view for the terms of service, with a
// formatted link to the full text of the terms of service.
- (UITextView*)createTermsOfServiceTextView {
  UITextView* textView = [[UITextView alloc] init];
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.delegate = self;
  textView.backgroundColor = UIColor.clearColor;
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  textView.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  NSDictionary* linkAttributes =
      @{NSLinkAttributeName : [NSURL URLWithString:kTermsOfServiceUrl]};
  // TODO(crbug.com/1189815): Use final strings and enable localization.
  NSAttributedString* attributedText = AttributedStringFromStringWithLink(
      @"Test - By continuing, you agree to the BEGIN_LINKTerms of "
      @"ServiceEND_LINK",
      textAttributes, linkAttributes);
  textView.attributedText = attributedText;

  return textView;
}

- (void)didTapMetricsButton {
  self.metricsConsentButton.selected = !self.metricsConsentButton.selected;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(textView == self.termsOfServiceTextView);
  [self.delegate didTapTOSLink];

  // The delegate is already handling the tap.
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the |selectedTextRange| to |nil| to prevent users from
  // selecting text. Setting the |selectable| property to |NO| doesn't help
  // since it makes links inside the text view untappable. Another solution is
  // to subclass |UITextView| and override |canBecomeFirstResponder| to return
  // NO, but that workaround only works on iOS 13.5+. This is the simplest
  // approach that works well on iOS 12, 13 & 14.
  textView.selectedTextRange = nil;
}

@end
