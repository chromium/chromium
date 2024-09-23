// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

// Keys in the lottie assets.
NSString* const kDefaultBrowserAppKeypath = @"IDS_DEFAULT_BROWSER_APP";
NSString* const kChromeKeypath = @"IDS_CHROME";

// Spacing used in the bottom alert view.
constexpr CGFloat kSpacing = 24;

// Vertical center offset for tablets.
constexpr CGFloat kTabletCenterOffset = 40;
}  // namespace

@interface DefaultBrowserInstructionsView ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

// Subview for information and action part of the view.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

@end

NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";

NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

@implementation DefaultBrowserInstructionsView

- (instancetype)
        initWithDismissButton:(BOOL)hasDismissButton
             hasRemindMeLater:(BOOL)hasRemindMeLater
                     hasSteps:(BOOL)hasSteps
                actionHandler:(id<ConfirmationAlertActionHandler>)actionHandler
    alertScreenViewController:(ConfirmationAlertViewController*)alertScreen
                    titleText:(NSString*)titleText {
  if ((self = [super init])) {
    CHECK(alertScreen);
    [self addVideoSection];
    [self addInformationSectionWithDismissButton:hasDismissButton
                                hasRemindMeLater:hasRemindMeLater
                                        hasSteps:hasSteps
                                   actionHandler:actionHandler
                       alertScreenViewController:alertScreen
                                       titleText:titleText];
    [self setBackgroundColor:[UIColor colorNamed:kGrey100Color]];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits =
          TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.self ]);
      [self registerForTraitChanges:traits
                         withAction:@selector(selectAnimationForCurrentStyle)];
    }
  }
  return self;
}

#pragma mark - UIViewController

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self selectAnimationForCurrentStyle];
}
#endif

#pragma mark - Private

// Adds the top part of the view which contains the video animation.
- (void)addVideoSection {
  NSString* animationAssetName;
  NSString* animationAssetNameDarkMode;

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
    kDefaultBrowserAppKeypath : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    kChromeKeypath : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME)
  };
  [self.animationViewWrapper setDictionaryTextProvider:textProvider];
  [self.animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  [self addSubview:self.animationViewWrapper.animationView];
  [self addSubview:self.animationViewWrapperDarkMode.animationView];

  // Layout the animation view to take up the top half of the view.
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
        constraintEqualToAnchor:self.leadingAnchor],
    [self.animationViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.animationViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.topAnchor],
    [self.animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.centerYAnchor
                       constant:[self centerOffset]],
  ]];

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

// Adds the bottom section of the view which contains instructions and buttons.
// If `titleText` is nil, default title will be used.
- (void)addInformationSectionWithDismissButton:(BOOL)hasDismissButton
                              hasRemindMeLater:(BOOL)hasRemindMeLater
                                      hasSteps:(BOOL)hasSteps
                                 actionHandler:
                                     (id<ConfirmationAlertActionHandler>)
                                         actionHandler
                     alertScreenViewController:
                         (ConfirmationAlertViewController*)alertScreen
                                     titleText:(NSString*)titleText {
  alertScreen.actionHandler = actionHandler;
  if (!titleText) {
    alertScreen.titleString =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT);
  } else {
    alertScreen.titleString = titleText;
  }
  alertScreen.primaryActionString = l10n_util ::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  alertScreen.imageHasFixedSize = YES;
  alertScreen.showDismissBarButton = NO;
  alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  alertScreen.customSpacingBeforeImageIfNoNavigationBar = kSpacing;
  alertScreen.topAlignedLayout = YES;
  alertScreen.customSpacingAfterImage = kSpacing;
  alertScreen.customSpacing = kSpacing;

  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    alertScreen.actionStackBottomMargin = kSpacing;
  }

  // The view can have either instruction steps or subtitles.
  if (hasSteps) {
    NSArray* defaultBrowserSteps = @[
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)
    ];

    UIView* instructionView =
        [[InstructionView alloc] initWithList:defaultBrowserSteps];
    instructionView.translatesAutoresizingMaskIntoConstraints = NO;

    alertScreen.underTitleView = instructionView;
    alertScreen.shouldFillInformationStack = YES;
  } else {
    alertScreen.subtitleString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SUBTITLE_TEXT);
  }

  if (hasDismissButton) {
    alertScreen.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_SECONDARY_BUTTON_TEXT);
  }

  if (hasRemindMeLater) {
    alertScreen.tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_TERTIARY_BUTTON_TEXT);
  }

  [self addSubview:alertScreen.view];

  // Layout the alert view to take up bottom half of the view.
  alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [alertScreen.view.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [alertScreen.view.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [alertScreen.view.widthAnchor constraintEqualToAnchor:self.widthAnchor],
    [alertScreen.view.topAnchor constraintEqualToAnchor:self.centerYAnchor
                                               constant:[self centerOffset]],
  ]];
  self.alertScreen = alertScreen;
}

// Returns the center offset for video instructions and information section to
// align with.
- (CGFloat)centerOffset {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return -kTabletCenterOffset;
  }
  return 0;
}

@end
