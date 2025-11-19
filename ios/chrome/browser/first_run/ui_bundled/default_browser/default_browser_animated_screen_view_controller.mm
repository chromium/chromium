// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_animated_screen_view_controller.h"

#import "base/feature_list.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Video animation asset names.
NSString* const kDefaultBrowserAnimation = @"default_browser_animation";
NSString* const kDefaultBrowserAnimationRtl = @"default_browser_animation_rtl";
NSString* const kDefaultBrowserAnimationDarkmode =
    @"default_browser_animation_darkmode";
NSString* const kDefaultBrowserAnimationRtlDarkmode =
    @"default_browser_animation_rtl_darkmode";
NSString* const kDefaultBrowserDefaultAppsAnimation =
    @"default_browser_default_apps_animation";
NSString* const kDefaultBrowserDefaultAppsAnimationRtl =
    @"default_browser_default_apps_animation_rtl";
NSString* const kDefaultBrowserDefaultAppsAnimationDarkmode =
    @"default_browser_default_apps_animation_darkmode";
NSString* const kDefaultBrowserDefaultAppsAnimationRtlDarkmode =
    @"default_browser_default_apps_animation_rtl_darkmode";

// Accessibility IDs.
NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";
NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

// Keys in the lottie assets.
NSString* const kBrowserAppKeypath = @"IDS_BROWSER_APP";
NSString* const kDefaultBrowserAppKeypath = @"IDS_DEFAULT_BROWSER_APP";
NSString* const kChromeKeypath = @"IDS_CHROME";

// Spacing for the content stack view.
const CGFloat kStackViewSpacing = 10;

// Spacing above the title.
const CGFloat kTitleTopMarginWhenNoHeaderImage = 30;
}  // namespace

@implementation DefaultBrowserAnimatedScreenViewController {
  // Custom animation view.
  id<LottieAnimation> _animationViewWrapper;
  // Custom animation view in dark mode.
  id<LottieAnimation> _animationViewWrapperDarkMode;
}

@synthesize hasPlatformPolicies = _hasPlatformPolicies;
@synthesize screenIntent = _screenIntent;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunAnimatedDefaultBrowserScreenAccessibilityIdentifier;
  self.subtitleBottomMargin = 0;
  self.titleTopMarginWhenNoHeaderImage = kTitleTopMarginWhenNoHeaderImage;
  self.preferToCompressContent = YES;

  NSArray<UITrait>* traits =
      TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
  [self registerForTraitChanges:traits
                     withAction:@selector(selectAnimationForCurrentStyle)];

  if (![self.titleText length] || ![self.subtitleText length]) {
    // Sets default promo text if title and subtitle text are not explicitly
    // set.
    CHECK(![self.titleText length]);
    CHECK(![self.subtitleText length]);
    BOOL usesTabletStrings =
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
    [self setPromoTitle:
              l10n_util::GetNSString(
                  usesTabletStrings
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)];
    [self setPromoSubtitle:
              l10n_util::GetNSString(
                  usesTabletStrings
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE)];
  }
  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_PRIMARY_ACTION);
  self.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);

  if (first_run::AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() ==
      first_run::AnimatedDefaultBrowserPromoInFREExperimentType::
          kAnimationWithShowMeHow) {
    self.configuration.tertiaryActionString =
        l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);
  }

  BOOL useDefaultAppsDestination =
      IsDefaultAppsDestinationAvailable() &&
      IsUseDefaultAppsDestinationForPromosEnabled();
  UIStackView* contentStack = [self contentStack:useDefaultAppsDestination];
  [self.specificContentView addSubview:contentStack];
  [NSLayoutConstraint activateConstraints:@[
    [contentStack.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [contentStack.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
    [contentStack.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [contentStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView.topAnchor]
  ]];

  [super viewDidLoad];
}

#pragma mark - DefaultBrowserScreenConsumer

- (void)setPromoTitle:(NSString*)titleText {
  self.titleText = titleText;
}

- (void)setPromoSubtitle:(NSString*)subtitleText {
  self.subtitleText = subtitleText;
}

#pragma mark - Private

// Returns a content stack that includes the animation and instructions views.
- (UIStackView*)contentStack:(BOOL)useDefaultAppsDestination {
  [self createAnimationViews:useDefaultAppsDestination];
  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _animationViewWrapper.animationView,
    _animationViewWrapperDarkMode.animationView
  ]];
  [NSLayoutConstraint activateConstraints:@[
    [_animationViewWrapper.animationView.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor],
    [_animationViewWrapperDarkMode.animationView.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor],
  ]];

  if ([self shouldShowInstructions]) {
    UIView* instructionsView =
        [self instructionsView:useDefaultAppsDestination];
    [stack addArrangedSubview:instructionsView];
    [NSLayoutConstraint activateConstraints:@[
      [instructionsView.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
    ]];
  }

  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = kStackViewSpacing;

  return stack;
}

// Creates the animation views. Sets `_animationViewWrapper` and
// `_animationViewWrapperDarkMode`.
- (void)createAnimationViews:(BOOL)useDefaultAppsDestination {
  NSString* animationAssetName = nil;
  NSString* animationAssetNameDarkMode = nil;

  if (useDefaultAppsDestination) {
    if (base::i18n::IsRTL()) {
      animationAssetName = kDefaultBrowserDefaultAppsAnimationRtl;
      animationAssetNameDarkMode =
          kDefaultBrowserDefaultAppsAnimationRtlDarkmode;
    } else {
      animationAssetName = kDefaultBrowserDefaultAppsAnimation;
      animationAssetNameDarkMode = kDefaultBrowserDefaultAppsAnimationDarkmode;
    }
  } else {
    if (base::i18n::IsRTL()) {
      animationAssetName = kDefaultBrowserAnimationRtl;
      animationAssetNameDarkMode = kDefaultBrowserAnimationRtlDarkmode;
    } else {
      animationAssetName = kDefaultBrowserAnimation;
      animationAssetNameDarkMode = kDefaultBrowserAnimationDarkmode;
    }
  }

  _animationViewWrapper = [self createAnimation:animationAssetName];
  _animationViewWrapper.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewAnimationViewId;
  _animationViewWrapperDarkMode =
      [self createAnimation:animationAssetNameDarkMode];
  _animationViewWrapperDarkMode.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewDarkAnimationViewId;

  // Set the text localization.
  NSDictionary* textProvider = @{
    kBrowserAppKeypath :
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_BROWSER_APP),
    kDefaultBrowserAppKeypath : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    kChromeKeypath : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME)
  };
  [_animationViewWrapper setDictionaryTextProvider:textProvider];
  [_animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;

  // Set low compression resistance priority for the animation views to make
  // their height dynamic.
  [_animationViewWrapper.animationView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  [_animationViewWrapperDarkMode.animationView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  [self selectAnimationForCurrentStyle];
}

// Returns an InstructionView configured with steps to set default browser.
- (UIView*)instructionsView:(BOOL)useDefaultAppsDestination {
  NSMutableArray* defaultBrowserSteps = [[NSMutableArray alloc] init];
  if (useDefaultAppsDestination) {
    [defaultBrowserSteps
        addObject:
            l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_FIRST_STEP)];
    [defaultBrowserSteps
        addObject:
            l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_SECOND_STEP)];
  } else {
    [defaultBrowserSteps
        addObject:l10n_util::GetNSString(
                      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP)];
    [defaultBrowserSteps
        addObject:l10n_util::GetNSString(
                      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP)];
  }
  [defaultBrowserSteps
      addObject:l10n_util::GetNSString(
                    IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)];

  UIView* instructionView =
      [[InstructionView alloc] initWithList:defaultBrowserSteps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  return instructionView;
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

// Selects regular or dark mode animation based on the given style.
- (void)selectAnimationForStyle:(UIUserInterfaceStyle)style {
  if (style == UIUserInterfaceStyleDark) {
    _animationViewWrapper.animationView.hidden = YES;
    [_animationViewWrapper stop];
    _animationViewWrapperDarkMode.animationView.hidden = NO;
    [_animationViewWrapperDarkMode play];
  } else {
    _animationViewWrapperDarkMode.animationView.hidden = YES;
    [_animationViewWrapperDarkMode stop];
    _animationViewWrapper.animationView.hidden = NO;
    [_animationViewWrapper play];
  }
}

// Selects the animation based on current dark mode settings.
- (void)selectAnimationForCurrentStyle {
  [self selectAnimationForStyle:self.traitCollection.userInterfaceStyle];
}

// Returns whether the Instruction View is included on the Default Browser
// Promo.
- (BOOL)shouldShowInstructions {
  return (first_run::AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() ==
          first_run::AnimatedDefaultBrowserPromoInFREExperimentType::
              kAnimationWithInstructions);
}

@end
