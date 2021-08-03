// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/browser/ui/first_run/welcome/tos_commands.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kDefaultMargin = 16;

// URL for the terms of service text.
NSString* const kTermsOfServiceUrl = @"internal://terms-of-service";

NSString* const kEnterpriseIconImageName = @"enterprise_icon";

}  // namespace

@interface WelcomeScreenViewController () <UITextViewDelegate>

@property(nonatomic, strong) CheckboxButton* metricsConsentButton;
@property(nonatomic, strong) UITextView* termsOfServiceTextView;
@property(nonatomic, weak) id<TOSCommands> TOSHandler;

@end

@implementation WelcomeScreenViewController
@dynamic delegate;

- (instancetype)initWithTOSHandler:(id<TOSCommands>)TOSHandler {
  DCHECK(TOSHandler);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _TOSHandler = TOSHandler;
  }
  return self;
}

- (void)viewDidLoad {
  [self configureLabels];
  self.view.accessibilityIdentifier =
      first_run::kFirstRunWelcomeScreenAccessibilityIdentifier;
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

  if ([self isBrowserManaged]) {
    UILabel* managedLabel = [self createManagedLabel];
    UIImage* image = [UIImage imageNamed:kEnterpriseIconImageName];
    UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
    imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.specificContentView addSubview:managedLabel];
    [self.specificContentView addSubview:imageView];

    [NSLayoutConstraint activateConstraints:@[
      [managedLabel.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [managedLabel.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [managedLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],

      [imageView.topAnchor constraintEqualToAnchor:managedLabel.bottomAnchor
                                          constant:kDefaultMargin],
      [imageView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],

      [self.metricsConsentButton.topAnchor
          constraintGreaterThanOrEqualToAnchor:imageView.bottomAnchor
                                      constant:kDefaultMargin],
    ]];
  } else {
    [self.metricsConsentButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView.topAnchor]
        .active = YES;
  }

  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.titleLabel);
}

#pragma mark - Accessors

- (BOOL)checkBoxSelected {
  return self.metricsConsentButton.selected;
}

#pragma mark - Private

// Configures the text for the title and subtitle based on whether the browser
// is managed or not.
- (void)configureLabels {
  if ([self isBrowserManaged]) {
    self.titleText = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_ENTERPRISE);
    self.subtitleText = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE_ENTERPRISE);
  } else {
    self.titleText =
        (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
            ? l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
            : l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
    self.subtitleText =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
  }
}

// Creates and configures the label for the disclaimer that the browser is
// managed.
- (UILabel*)createManagedLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  label.numberOfLines = 0;
  label.text = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_MANAGED);
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

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
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
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

// Handler for when the metrics button gets tapped. Toggles the button's
// selected state.
- (void)didTapMetricsButton {
  self.metricsConsentButton.selected = !self.metricsConsentButton.selected;
}

// Returns whether the browser is managed based on the presence of policy data
// in the app configuration.
- (BOOL)isBrowserManaged {
  return [[[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] count] > 0;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(textView == self.termsOfServiceTextView);
  [self.TOSHandler showTOSPage];

  // The handler is already handling the tap.
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
