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
// Accessibility IDs.
NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";
NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";
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

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(selectAnimationForCurrentStyle)];
  }

  if (![self.titleText length] || ![self.subtitleText length]) {
    // Sets default promo text if title and subtitle text are not explicitly
    // set.
    CHECK(![self.titleText length], base::NotFatalUntil::M138);
    CHECK(![self.subtitleText length], base::NotFatalUntil::M138);
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
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_PRIMARY_ACTION);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);

  if (first_run::AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() ==
      first_run::AnimatedDefaultBrowserPromoInFREExperimentType::
          kAnimationWithShowMeHow) {
    self.tertiaryActionString =
        l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);
  }

  UIStackView* contentStack = [self contentStack];
  [self.specificContentView addSubview:contentStack];
  AddSameConstraints(contentStack, self.specificContentView);

  [super viewDidLoad];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self selectAnimationForCurrentStyle];
}
#endif

#pragma mark - DefaultBrowserScreenConsumer

- (void)setPromoTitle:(NSString*)titleText {
  self.titleText = titleText;
}

- (void)setPromoSubtitle:(NSString*)subtitleText {
  self.subtitleText = subtitleText;
}

#pragma mark - Private

// Returns a content stack that includes the animation and instructions views.
- (UIStackView*)contentStack {
  [self createAnimationViews];
  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _animationViewWrapper.animationView,
    _animationViewWrapperDarkMode.animationView
  ]];

  if ([self shouldShowInstructions]) {
    UIView* instructionsView = [self instructionsView];
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
- (void)createAnimationViews {
  NSString* animationAssetName = nil;
  NSString* animationAssetNameDarkMode = nil;

  // TODO(crbug.com/40948842): Handle the case when the promo is displayed and
  // the user switches between LTR and RLT.
  if (base::i18n::IsRTL()) {
    animationAssetName = kDefaultBrowserAnimationRtl;
    animationAssetNameDarkMode = kDefaultBrowserAnimationRtlDarkmode;
  } else {
    animationAssetName = kDefaultBrowserAnimation;
    animationAssetNameDarkMode = kDefaultBrowserAnimationDarkmode;
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
    @"IDS_IOS_DEFAULT_BROWSER_APP" : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    @"IDS_CHROME" : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME)
  };
  [_animationViewWrapper setDictionaryTextProvider:textProvider];
  [_animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;

  // Set low compression resistance priority for the animation views to make
  // their height dynamic. We need to index the subviews here because Lottie
  // animation wrapper doesn't expose the view we need access to for this.
  // TODO(crbug.com/404301564): Expose the subview we need from Lottie.
  NSArray<UIView*>* animationSubviews =
      _animationViewWrapper.animationView.subviews;
  NSArray<UIView*>* animationDarkmodeSubviews =
      _animationViewWrapper.animationView.subviews;
  CHECK([animationSubviews count] == 1, base::NotFatalUntil::M138);
  CHECK([animationDarkmodeSubviews count] == 1, base::NotFatalUntil::M138);
  [animationSubviews[0]
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  [animationDarkmodeSubviews[0]
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  [self selectAnimationForCurrentStyle];
}

// Returns an InstructionView configured with steps to set default browser.
- (UIView*)instructionsView {
  NSArray* defaultBrowserSteps = @[
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)
  ];

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
  config.loopAnimationCount = -1;  // Always loop.
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
