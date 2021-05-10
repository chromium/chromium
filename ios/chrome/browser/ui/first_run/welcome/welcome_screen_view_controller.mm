// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
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
  self.titleText =
      IsIPadIdiom()
          ? l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
          : l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
  self.bannerImage = [UIImage imageNamed:@"welcome_screen_banner"];
  self.isTallBanner = YES;
  self.scrollToEndMandatory = YES;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON);

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
  button.labelText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRICS_CONSENT);
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
  NSAttributedString* attributedText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
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
