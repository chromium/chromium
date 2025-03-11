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

// The spacing between the bottom of the `AnimationView` and the top of the
// `InstructionView`.
constexpr CGFloat kAnimationOffsetFromInstruction = -10;

// The `AnimationView` height offest for wide devices.
constexpr CGFloat kWideAnimationHeightOffset = -200;

// The `AnimationView` top offset for wide devices.
constexpr CGFloat kWidetopAnchorOffset = 125;

// Vertical center offset for tablets.
constexpr CGFloat kTabletCenterOffset = 40;
}  // namespace

@interface DefaultBrowserAnimatedScreenViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

// Subview for information and action part of the view.
@property(nonatomic, strong) PromoStyleViewController* promoScreen;

@end

NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";

NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

@implementation DefaultBrowserAnimatedScreenViewController {
  UIView* _instructionView;
}

@synthesize hasPlatformPolicies = _hasPlatformPolicies;
@synthesize screenIntent = _screenIntent;

#pragma mark - UIViewController

- (void)viewDidLoad {
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(selectAnimationForCurrentStyle)];
  }

  self.view.accessibilityIdentifier =
      first_run::kFirstRunAnimatedDefaultBrowserScreenAccessibilityIdentifier;
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

  NSArray* defaultBrowserSteps = @[
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)
  ];

  // Only add the instruction view if the Animated DBP is disabled or the
  // appropriate experiment arm is enabled.
  if ([self shouldShowInstructions]) {
    UIView* instructionView =
        [[InstructionView alloc] initWithList:defaultBrowserSteps];
    instructionView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.specificContentView addSubview:instructionView];

    [NSLayoutConstraint activateConstraints:@[
      [instructionView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [instructionView.widthAnchor
          constraintEqualToAnchor:self.specificContentView.widthAnchor],
      [instructionView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [instructionView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                   .topAnchor],
    ]];
    _instructionView = instructionView;
  }

  [super viewDidLoad];

  // `addVideoSection` is called after `viewDidLoad` to ensure the necessary
  // parent views are loaded. These views are necessary before the constraints
  // for the video subview can be applied.
  [self addVideoSection];
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

// Adds the video animation.
- (void)addVideoSection {
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

  self.animationViewWrapper = [self createAnimation:animationAssetName];
  self.animationViewWrapper.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewAnimationViewId;
  self.animationViewWrapperDarkMode =
      [self createAnimation:animationAssetNameDarkMode];
  self.animationViewWrapperDarkMode.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewDarkAnimationViewId;

  // Set the text localization.
  NSDictionary* textProvider = @{
    @"IDS_IOS_DEFAULT_BROWSER_APP" : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    @"IDS_CHROME" : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME)
  };
  [self.animationViewWrapper setDictionaryTextProvider:textProvider];
  [self.animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  [self.specificContentView addSubview:self.animationViewWrapper.animationView];
  [self.specificContentView
      addSubview:self.animationViewWrapperDarkMode.animationView];

  // Layout the animation view.
  self.animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  self.animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.animationViewWrapperDarkMode.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [self.animationViewWrapper.animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.animationViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  CGFloat heightAnchorOffest = [self shouldShowInstructions]
                                   ? kWideAnimationHeightOffset * 2
                                   : kWideAnimationHeightOffset;

  if ([self shouldShowInstructions]) {
    [NSLayoutConstraint activateConstraints:@[
      [self.animationViewWrapper.animationView.bottomAnchor
          constraintEqualToAnchor:_instructionView.topAnchor
                         constant:kAnimationOffsetFromInstruction],
      [self.animationViewWrapper.animationView.heightAnchor
          constraintEqualToAnchor:self.view.heightAnchor
                         constant:heightAnchorOffest],
    ]];
  }

  if ([self isDisplayRegular]) {
    [NSLayoutConstraint activateConstraints:@[
      [self.animationViewWrapper.animationView.heightAnchor
          constraintEqualToAnchor:self.view.heightAnchor
                         constant:heightAnchorOffest],
      [self.animationViewWrapper.animationView.topAnchor
          constraintEqualToAnchor:self.view.topAnchor
                         constant:kWidetopAnchorOffset],
      [self.animationViewWrapper.animationView.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
    ]];
  }

  AddSameConstraints(self.animationViewWrapperDarkMode.animationView,
                     self.animationViewWrapper.animationView);

  [self selectAnimationForCurrentStyle];
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
    self.animationViewWrapper.animationView.hidden = YES;
    [self.animationViewWrapper stop];

    self.animationViewWrapperDarkMode.animationView.hidden = NO;
    [self.animationViewWrapperDarkMode play];
  } else {
    self.animationViewWrapperDarkMode.animationView.hidden = YES;
    [self.animationViewWrapperDarkMode stop];

    self.animationViewWrapper.animationView.hidden = NO;
    [self.animationViewWrapper play];
  }
}

// Selects the animation based on current dark mode settings.
- (void)selectAnimationForCurrentStyle {
  [self selectAnimationForStyle:self.traitCollection.userInterfaceStyle];
}

// Returns the center offset for video instructions and information section to
// align with.
- (CGFloat)centerOffset {
  if ([self isDisplayRegular]) {
    return -kTabletCenterOffset;
  }
  return 0;
}

// Returns whether the Instruction View is included on the Default Browser
// Promo.
- (BOOL)shouldShowInstructions {
  return (first_run::AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() ==
          first_run::AnimatedDefaultBrowserPromoInFREExperimentType::
              kAnimationWithInstructions);
}

// Returns whether the horizontal display is regular. Used to determine the
// layout for larger screens.
- (BOOL)isDisplayRegular {
  return self.traitCollection.horizontalSizeClass ==
         UIUserInterfaceSizeClassRegular;
}

@end
