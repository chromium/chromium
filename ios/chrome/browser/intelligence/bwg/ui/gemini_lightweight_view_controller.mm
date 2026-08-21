// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_lightweight_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Medium spacing for title/content and accordion/footnote gaps.
const CGFloat kMediumSpacing = 20.0;

}  // namespace

@interface GeminiLightweightViewController () <
    GeminiConsentAccordionViewDelegate,
    UITextViewDelegate>
@end

@implementation GeminiLightweightViewController {
  // Consent configuration containing rows, footnote, and strict mode.
  GeminiConsentConfiguration* _configuration;
  // Main stack view containing all content.
  UIStackView* _mainStackView;
  // Label containing the main title.
  UILabel* _mainTitleLabel;
  // Accordion view displaying the consent items.
  GeminiConsentAccordionView* _accordionView;
  // Container view for the footnote.
  UIView* _footnoteContainer;
  // Text view displaying the footnote.
  UITextView* _footnoteView;
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
  // Cap the max text size to make sure that part of the consent will be seen
  // regardless of the user's font size settings.
  self.view.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;
  [self setupSubviews];
  [self setupConstraints];
}

#pragma mark - GeminiFirstRunStep

- (CGFloat)contentHeight {
  CGFloat width = self.view.bounds.size.width;
  _mainTitleLabel.preferredMaxLayoutWidth = width;
  return [GeminiUIUtils contentHeightForView:_mainStackView
                          withContainerWidth:width];
}

- (GeminiFirstRunStepIdentifier)stepIdentifier {
  return GeminiFirstRunStepIdentifier::kLightweight;
}

- (ButtonStackConfiguration*)buttonStackConfiguration {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BWG_VISUAL_RICH_PRIMARY_BUTTON);
  configuration.secondaryActionString =
      _configuration.useStrict
          ? l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON)
          : l10n_util::GetNSString(IDS_CANCEL);
  return configuration;
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
  // No-op.
}

- (void)didTapPrimaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kAccept);
  [self.mutator didConsentGemini];
}

- (void)didTapSecondaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kDismiss);
  [self.mutator didRefuseGeminiConsent];
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return nil;
  }

  NSURL* url = textItem.link;
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.mutator didTapConsentLinkWithAction:url.absoluteString];
  }];
}

- (UITextItemMenuConfiguration*)textView:(UITextView*)textView
            menuConfigurationForTextItem:(UITextItem*)textItem
                             defaultMenu:(UIMenu*)defaultMenu {
  return nil;
}

#pragma mark - GeminiConsentAccordionViewDelegate

- (void)accordionView:(GeminiConsentAccordionView*)view didTapLink:(NSURL*)url {
  [self.mutator didTapConsentLinkWithAction:url.absoluteString];
}

- (void)accordionView:(GeminiConsentAccordionView*)view
         didToggleRow:(GeminiConsentRow*)row {
  [self.stepDelegate stepContentHeightDidChange:self];
}

#pragma mark - Private

- (void)setupSubviews {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];

  // Main title.
  _mainTitleLabel = [self createMainTitleLabel];
  [_mainStackView addArrangedSubview:_mainTitleLabel];
  [_mainStackView setCustomSpacing:kMediumSpacing afterView:_mainTitleLabel];

  // Consent accordion view.
  _accordionView = [[GeminiConsentAccordionView alloc]
      initWithRows:_configuration.rows
       collapsible:_configuration.collapsible];
  _accordionView.delegate = self;
  [_mainStackView addArrangedSubview:_accordionView];

  // Optional footnote view.
  if (_configuration.footnote) {
    [_mainStackView setCustomSpacing:kMediumSpacing afterView:_accordionView];
    _footnoteContainer = [[UIView alloc] init];
    _footnoteContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _footnoteView = [GeminiUIUtils
        createFootnoteViewWithAttributedText:_configuration.footnote];
    _footnoteView.delegate = self;
    _footnoteView.translatesAutoresizingMaskIntoConstraints = NO;
    [_footnoteContainer addSubview:_footnoteView];
    [_mainStackView addArrangedSubview:_footnoteContainer];
  }
}

- (void)setupConstraints {
  PinToSafeArea(_mainStackView, self.view);

  if (_footnoteView && _footnoteContainer) {
    AddSameConstraints(_footnoteView, _footnoteContainer);
  }
}

- (UILabel*)createMainTitleLabel {
  UILabel* mainTitleLabel = [[UILabel alloc] init];
  mainTitleLabel.textAlignment = NSTextAlignmentCenter;
  mainTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  mainTitleLabel.numberOfLines = 0;
  mainTitleLabel.adjustsFontForContentSizeCategory = YES;
  mainTitleLabel.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;
  UIFont* labelFont =
      PreferredFontForTextStyle(UIFontTextStyleTitle1, UIFontWeightBold);
  mainTitleLabel.font = labelFont;

  NSString* mainTitleString = [self.mutator lightweightPromoTitle];
  mainTitleLabel.attributedText =
      [GeminiUIUtils attributedStringWithGradientGeminiForTitle:mainTitleString
                                                           font:labelFont];
  mainTitleLabel.accessibilityLabel = mainTitleString;
  mainTitleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  return mainTitleLabel;
}

@end
