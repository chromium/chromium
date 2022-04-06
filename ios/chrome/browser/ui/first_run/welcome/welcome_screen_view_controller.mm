// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#include "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/commands/tos_commands.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
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
constexpr CGFloat kEnterpriseIconBorderWidth = 1;
constexpr CGFloat kEnterpriseIconCornerRadius = 7.0;
constexpr CGFloat kEnterpriseIconContainerLength = 30;

// URL for the terms of service text.
NSString* const kTermsOfServiceURL = @"internal://terms-of-service";

// URL for the terms of service text.
NSString* const kManageMetricsReportedURL = @"internal://uma-manager";

NSString* const kEnterpriseIconImageName = @"enterprise_icon";

NSString* const kMetricsConsentCheckboxAccessibilityIdentifier =
    @"kMetricsConsentCheckboxAccessibilityIdentifier";

}  // namespace

@interface WelcomeScreenViewController () <UITextViewDelegate>

@property(nonatomic, strong) CheckboxButton* metricsConsentButton;
@property(nonatomic, strong) UITextView* footerTextView;
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
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON);

  self.metricsConsentButton = [self createMetricsConsentButton];
  UIView* footerTopAnchor = nil;
  BOOL showUMAReportingCheckBox =
      fre_field_trial::GetNewMobileIdentityConsistencyFRE() ==
      NewMobileIdentityConsistencyFRE::kOld;
  self.footerTextView =
      [self createFooterTextViewWithUMAReportingLink:!showUMAReportingCheckBox];
  [self.specificContentView addSubview:self.footerTextView];

  [NSLayoutConstraint activateConstraints:@[
    [self.footerTextView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.footerTextView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [self.footerTextView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  if (!showUMAReportingCheckBox) {
    footerTopAnchor = self.footerTextView;
  } else {
    self.metricsConsentButton = [self createMetricsConsentButton];
    [self.specificContentView addSubview:self.metricsConsentButton];
    footerTopAnchor = self.metricsConsentButton;
    [NSLayoutConstraint activateConstraints:@[
      [self.metricsConsentButton.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.metricsConsentButton.widthAnchor
          constraintEqualToAnchor:self.specificContentView.widthAnchor],
      [self.footerTextView.topAnchor
          constraintEqualToAnchor:self.metricsConsentButton.bottomAnchor
                         constant:kDefaultMargin]
    ]];
  }

  UIView* headerBottomAnchor = nil;
  if (![self isBrowserManaged]) {
    headerBottomAnchor = self.specificContentView;
  } else {
    UILabel* managedLabel = [self createManagedLabel];
    UIView* managedIcon = [self createManagedIcon];
    [self.specificContentView addSubview:managedLabel];
    [self.specificContentView addSubview:managedIcon];

    [NSLayoutConstraint activateConstraints:@[
      [managedLabel.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [managedLabel.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [managedLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],

      [managedIcon.topAnchor constraintEqualToAnchor:managedLabel.bottomAnchor
                                            constant:kDefaultMargin],
      [managedIcon.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    ]];
    headerBottomAnchor = managedIcon;
  }
  [footerTopAnchor.topAnchor
      constraintGreaterThanOrEqualToAnchor:headerBottomAnchor.topAnchor]
      .active = YES;
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

// Creates and configures the icon indicating that the browser is managed.
- (UIView*)createManagedIcon {
  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.layer.cornerRadius = kEnterpriseIconCornerRadius;
  iconContainer.layer.borderWidth = kEnterpriseIconBorderWidth;
  iconContainer.layer.borderColor = [UIColor colorNamed:kGrey200Color].CGColor;

  UIImage* image = [[UIImage imageNamed:kEnterpriseIconImageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.tintColor = [UIColor colorNamed:kGrey500Color];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  [iconContainer addSubview:imageView];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor
        constraintEqualToConstant:kEnterpriseIconContainerLength],
    [iconContainer.heightAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor],
    [imageView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [imageView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
  ]];

  return iconContainer;
}

// Creates and configures the UMA consent checkbox button.
- (CheckboxButton*)createMetricsConsentButton {
  CheckboxButton* button = [[CheckboxButton alloc] initWithFrame:CGRectZero];
  button.accessibilityIdentifier =
      kMetricsConsentCheckboxAccessibilityIdentifier;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.labelText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRICS_CONSENT);
  button.selected = YES;
  [button addTarget:self
                action:@selector(didTapMetricsButton)
      forControlEvents:UIControlEventTouchUpInside];

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Creates and configures the text view for the terms of service, with a
// formatted link to the full text of the terms of service.
- (UITextView*)createFooterTextViewWithUMAReportingLink:
    (BOOL)UMAReportingLink {
  NSAttributedString* termsOfServiceString = [self createTermsOfServiceString];
  NSMutableAttributedString* footerString = [[NSMutableAttributedString alloc]
      initWithAttributedString:termsOfServiceString];
  if (UMAReportingLink) {
    NSAttributedString* manageMetricsReported =
        [self createManageMetricsReportedString];
    [footerString appendAttributedString:manageMetricsReported];
  }

  UITextView* textView = [[UITextView alloc] init];
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.delegate = self;
  textView.backgroundColor = UIColor.clearColor;
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.attributedText = footerString;
  return textView;
}

- (NSAttributedString*)createTermsOfServiceString {
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
      @{NSLinkAttributeName : [NSURL URLWithString:kTermsOfServiceURL]};
  return AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
      textAttributes, linkAttributes);
}

//  Returns a NSAttributedString for UMA reporting with a link.
- (NSAttributedString*)createManageMetricsReportedString {
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
      @{NSLinkAttributeName : [NSURL URLWithString:kManageMetricsReportedURL]};
  NSMutableString* string = [[NSMutableString alloc] initWithString:@"\n"];
  NSString* manageString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING);
  [string appendString:manageString];
  return AttributedStringFromStringWithLink(string, textAttributes,
                                            linkAttributes);
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
  DCHECK(textView == self.footerTextView);
  NSString* URLString = URL.absoluteString;
  if (URLString == kTermsOfServiceURL) {
    [self.TOSHandler showTOSPage];
  } else if (URLString == kManageMetricsReportedURL) {
    [self.delegate showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }

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
