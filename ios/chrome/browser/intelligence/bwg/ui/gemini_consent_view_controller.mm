// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackSpacing = 16.0;

// Header traits.
const CGFloat kHeaderIconContainerCornerRadius = 16.0;
const CGFloat kHeaderIconContainerWidthMultiplier = 0.14;
const CGFloat kHeaderIconSizeMultiplier = 0.55;

}  // namespace

@interface GeminiConsentViewController () <GeminiConsentAccordionViewDelegate,
                                           UITextViewDelegate>
@end

@implementation GeminiConsentViewController {
  // Main stack view. This view itself does not scroll.
  UIStackView* _mainStackView;
  // The view data for this view controller.
  GeminiConsentConfiguration* _configuration;
}

- (instancetype)initWithConfiguration:
    (GeminiConsentConfiguration*)configuration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self configureMainStackView];
}

#pragma mark - GeminiFirstRunViewControllerProtocol

// Returns the expected content height of this view.
- (CGFloat)contentHeight {
  return [GeminiUIUtils contentHeightForView:_mainStackView
                          withContainerWidth:self.view.bounds.size.width];
}

#pragma mark - GeminiFirstRunStep

- (GeminiFirstRunStepIdentifier)stepIdentifier {
  return GeminiFirstRunStepIdentifier::kConsent;
}

+ (ButtonStackConfiguration*)buttonStackConfigurationForConfiguration:
    (GeminiConsentConfiguration*)configuration {
  ButtonStackConfiguration* buttonConfiguration =
      [[ButtonStackConfiguration alloc] init];
  BOOL useStrictButton =
      IsGeminiUpdatedConsentEnabled() && configuration.useStrict;
  buttonConfiguration.primaryActionString = l10n_util::GetNSString(
      useStrictButton ? IDS_IOS_GEMINI_CONSENT_PRIMARY_BUTTON_STRICT
                      : IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON);
  buttonConfiguration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON);
  return buttonConfiguration;
}

- (ButtonStackConfiguration*)buttonStackConfiguration {
  return [GeminiConsentViewController
      buttonStackConfigurationForConfiguration:_configuration];
}

- (void)stepDidBecomeActive {
  // If the related `WebState` was hidden asynchronously while the sheet was
  // appearing, the mutator becomes nil. Automatically dismiss to avoid leaving
  // a broken view.
  if (!self.mutator) {
    [self dismissViewControllerAnimated:YES completion:nil];
  }
}

- (void)stepWillResignActive {
  // No-op
}

- (void)didTapPrimaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kAccept);
  if (self.firstRunType == GeminiFirstRunType::kLive) {
    [self.mutator didConsentToLiveGemini];
  } else {
    [self.mutator didConsentGemini];
  }
}

- (void)didTapSecondaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kDismiss);
  [self.mutator didRefuseGeminiConsent];
}

#pragma mark - Private

// Configures the main stack view with the accordion, header and footnotes.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];
  AddSameConstraints(_mainStackView, self.view);

  if (_configuration.header) {
    [_mainStackView addArrangedSubview:[self createHeaderView]];
  }

  [_mainStackView addArrangedSubview:[self createAccordionView]];

  if (_configuration.footnote) {
    UITextView* footnoteView = [GeminiUIUtils
        createFootnoteViewWithAttributedText:_configuration.footnote];
    footnoteView.delegate = self;
    [_mainStackView addArrangedSubview:footnoteView];
  }
}

// Creates the header using the header configuration.
- (UIView*)createHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] init];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing = kMainStackSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

  [headerView addSubview:verticalStack];
  AddSameConstraintsWithInsets(verticalStack, headerView,
                               NSDirectionalEdgeInsetsZero);

  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  iconContainer.layer.cornerRadius = kHeaderIconContainerCornerRadius;

  [verticalStack addArrangedSubview:iconContainer];
  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor
        constraintEqualToAnchor:headerView.widthAnchor
                     multiplier:kHeaderIconContainerWidthMultiplier],
    [iconContainer.heightAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor]
  ]];

  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.image = _configuration.header.icon;
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.translatesAutoresizingMaskIntoConstraints = NO;

  [iconContainer addSubview:iconView];
  [NSLayoutConstraint activateConstraints:@[
    [iconView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [iconView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
    [iconView.widthAnchor constraintEqualToAnchor:iconContainer.widthAnchor
                                       multiplier:kHeaderIconSizeMultiplier],
    [iconView.heightAnchor constraintEqualToAnchor:iconContainer.heightAnchor
                                        multiplier:kHeaderIconSizeMultiplier]
  ]];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.attributedText = _configuration.header.title;
  [verticalStack addArrangedSubview:titleLabel];

  return headerView;
}

// Creates the accordion view using GeminiConsentAccordionView.
- (UIView*)createAccordionView {
  GeminiConsentAccordionView* accordionView =
      [[GeminiConsentAccordionView alloc]
          initWithRows:_configuration.rows
           collapsible:_configuration.collapsible];
  accordionView.delegate = self;
  accordionView.translatesAutoresizingMaskIntoConstraints = NO;

  return accordionView;
}

#pragma mark - UITextViewDelegate

// Handles tap on UITextView.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return nil;
  }

  NSString* actionString = textItem.link.absoluteString;
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.mutator didTapConsentLinkWithAction:actionString];
  }];
}

// If the text item is a link, return nil to prevent the long-press context menu
// from appearing.
- (UIMenu*)textView:(UITextView*)textView
    menuConfigurationForTextItem:(UITextItem*)textItem
                     defaultMenu:(UIMenu*)defaultMenu {
  if (textItem.link) {
    return nil;
  }
  return defaultMenu;
}

#pragma mark - GeminiConsentAccordionViewDelegate

- (void)accordionView:(GeminiConsentAccordionView*)view didTapLink:(NSURL*)url {
  [self.mutator didTapConsentLinkWithAction:url.absoluteString];
}

- (void)accordionView:(GeminiConsentAccordionView*)view
         didToggleRow:(GeminiConsentRow*)row {
  if (IsGeminiFRERefactorEnabled()) {
    // Notify the container to recalculate bottom sheet detents for the new
    // content height.
    [self.stepDelegate stepContentHeightDidChange:self];
  } else {
    [self.delegate consentViewControllerDidExpandAccordionItem:self];
  }
}

@end
