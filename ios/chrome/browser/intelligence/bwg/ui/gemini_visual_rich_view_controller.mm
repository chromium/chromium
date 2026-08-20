// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_visual_rich_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_view.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Small spacing constant for compact layout and footnote spacing.
const CGFloat kSpacingSmall = 16.0;
// Medium spacing constant between carousel view and consent accordion.
const CGFloat kSpacingMedium = 36.0;
// Top spacing padding before the carousel view.
const CGFloat kTopPadding = 42.0;
// Top spacing padding before the carousel view in compact height mode.
const CGFloat kCompactTopPadding = 27.0;
}  // namespace

@interface GeminiVisualRichViewController () <
    GeminiConsentAccordionViewDelegate,
    UITextViewDelegate>
@end

@implementation GeminiVisualRichViewController {
  GeminiConsentConfiguration* _configuration;
  UIStackView* _mainStackView;
  GeminiFirstRunCarouselView* _carouselView;
  GeminiConsentAccordionView* _accordionView;
  NSLayoutConstraint* _topConstraint;
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
  [self configureMainStackView];

  // Register for vertical size class changes to dynamically update spacing in
  // compact height (landscape) mode.
  [self
      registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                   withAction:@selector(updateLayoutForCurrentTraitCollection)];
  [self updateLayoutForCurrentTraitCollection];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  // Prepare and animate the carousel view across size and orientation changes
  // to ensure the currently active slide remains centered.
  GeminiFirstRunCarouselView* carouselView = _carouselView;
  [carouselView prepareForSizeTransition];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [carouselView recenterActiveSlide];
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [carouselView completeSizeTransition];
      }];
}

#pragma mark - GeminiFirstRunStep

- (BOOL)shouldUseFullscreenPresentation {
  return YES;
}

- (CGFloat)contentHeight {
  BOOL isCompactHeight = (self.traitCollection.verticalSizeClass ==
                          UIUserInterfaceSizeClassCompact);
  CGFloat topPadding = isCompactHeight ? kCompactTopPadding : kTopPadding;
  return [GeminiUIUtils contentHeightForView:_mainStackView
                          withContainerWidth:self.view.bounds.size.width] +
         topPadding;
}

- (GeminiFirstRunStepIdentifier)stepIdentifier {
  return GeminiFirstRunStepIdentifier::kVisualRich;
}

- (ButtonStackConfiguration*)buttonStackConfiguration {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BWG_VISUAL_RICH_PRIMARY_BUTTON);
  configuration.secondaryActionString = l10n_util::GetNSString(
      _configuration.useStrict ? IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON
                               : IDS_CANCEL);
  return configuration;
}

- (void)stepDidBecomeActive {
  [_carouselView startAutoScrolling];
}

- (void)stepWillResignActive {
  [_carouselView stopAutoScrolling];
}

- (void)didTapPrimaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kAccept);
  [self.mutator didConsentGemini];
}

- (void)didTapSecondaryButton {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kDismiss);
  [self.mutator didRefuseGeminiConsent];
}

#pragma mark - GeminiConsentAccordionViewDelegate

- (void)accordionView:(GeminiConsentAccordionView*)view didTapLink:(NSURL*)url {
  [self.mutator didTapConsentLinkWithAction:url.absoluteString];
}

- (void)accordionView:(GeminiConsentAccordionView*)view
         didToggleRow:(GeminiConsentRow*)row {
  [self.stepDelegate stepContentHeightDidChange:self];
}

#pragma mark - UITextViewDelegate

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

- (UITextItemMenuConfiguration*)textView:(UITextView*)textView
            menuConfigurationForTextItem:(UITextItem*)textItem
                             defaultMenu:(UIMenu*)defaultMenu {
  return nil;
}

#pragma mark - Private

- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kSpacingMedium;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];

  _topConstraint =
      [_mainStackView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                               constant:kTopPadding];

  [NSLayoutConstraint activateConstraints:@[
    _topConstraint,
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_mainStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // Instantiate carousel view with FRE slides.
  NSString* suffix =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_CAROUSEL_GEMINI_IN_CHROME);
  NSString* summarizeTitle = [NSString
      stringWithFormat:@"%@\n%@",
                       l10n_util::GetNSString(
                           IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE),
                       suffix];
  NSString* shoppingTitle =
      [NSString stringWithFormat:@"%@ %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_TITLE),
                                 suffix];
  NSString* planningTitle =
      [NSString stringWithFormat:@"%@ %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE),
                                 suffix];

  NSArray<GeminiFirstRunCarouselSlide*>* slides = @[
    [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFRESummarizeSlideName
                  darkAnimationName:kLottieAnimationFRESummarizeSlideDarkName
                   animationNameRTL:kLottieAnimationFRESummarizeSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFRESummarizeSlideDarkRTLName
                              title:summarizeTitle
        animationAccessibilityLabel:
            l10n_util::GetNSString(
                IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_ANIMATION_ACCESSIBILITY_LABEL)],
    [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFREShoppingSlideName
                  darkAnimationName:kLottieAnimationFREShoppingSlideDarkName
                   animationNameRTL:kLottieAnimationFREShoppingSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFREShoppingSlideDarkRTLName
                              title:shoppingTitle
        animationAccessibilityLabel:
            l10n_util::GetNSString(
                IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_ANIMATION_ACCESSIBILITY_LABEL)],
    [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFREPlanningSlideName
                  darkAnimationName:kLottieAnimationFREPlanningSlideDarkName
                   animationNameRTL:kLottieAnimationFREPlanningSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFREPlanningSlideDarkRTLName
                              title:planningTitle
        animationAccessibilityLabel:
            l10n_util::GetNSString(
                IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_ANIMATION_ACCESSIBILITY_LABEL)],
  ];

  _carouselView = [[GeminiFirstRunCarouselView alloc] initWithSlides:slides];
  [_carouselView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [_mainStackView addArrangedSubview:_carouselView];

  // Consent bullet points accordion view.
  _accordionView = [[GeminiConsentAccordionView alloc]
      initWithRows:_configuration.rows
       collapsible:_configuration.collapsible];
  _accordionView.delegate = self;
  [_mainStackView addArrangedSubview:_accordionView];

  // Optional footnote view.
  if (_configuration.footnote) {
    [_mainStackView setCustomSpacing:kSpacingSmall afterView:_accordionView];
    UITextView* footnoteView = [GeminiUIUtils
        createFootnoteViewWithAttributedText:_configuration.footnote];
    footnoteView.delegate = self;
    [_mainStackView addArrangedSubview:footnoteView];
  }
}

- (void)updateLayoutForCurrentTraitCollection {
  BOOL isCompactHeight = (self.traitCollection.verticalSizeClass ==
                          UIUserInterfaceSizeClassCompact);
  CGFloat spacing = isCompactHeight ? kSpacingSmall : kSpacingMedium;
  [_mainStackView setCustomSpacing:spacing afterView:_carouselView];
  _topConstraint.constant = isCompactHeight ? kCompactTopPadding : kTopPadding;
}

@end
