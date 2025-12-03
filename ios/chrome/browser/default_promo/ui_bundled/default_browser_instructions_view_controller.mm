// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/default_promo/ui_bundled/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
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
NSString* const kDefaultBrowserAnimationIpad =
    @"default_browser_animation_ipad";
NSString* const kDefaultBrowserAnimationRtlIpad =
    @"default_browser_animation_rtl_ipad";
NSString* const kDefaultBrowserAnimationDarkmodeIpad =
    @"default_browser_animation_darkmode_ipad";
NSString* const kDefaultBrowserAnimationRtlDarkmodeIpad =
    @"default_browser_animation_rtl_darkmode_ipad";

// Keys in the lottie assets.
NSString* const kBrowserAppKeypath = @"IDS_BROWSER_APP";
NSString* const kDefaultBrowserAppKeypath = @"IDS_DEFAULT_BROWSER_APP";
NSString* const kChromeKeypath = @"IDS_CHROME";

// Spacing used in the bottom alert view.
constexpr CGFloat kSpacing = 24;

// Vertical center offset for tablets.
constexpr CGFloat kTabletCenterOffset = 40;
}  // namespace

@interface DefaultBrowserInstructionsViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

// Subview for information and action part of the view.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";

NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

@implementation DefaultBrowserInstructionsViewController {
  BOOL _useDefaultAppsDestination;
}

- (instancetype)initWithDismissButton:(BOOL)hasDismissButton
                     hasRemindMeLater:(BOOL)hasRemindMeLater
            useDefaultAppsDestination:(BOOL)useDefaultAppsDestination
                             hasSteps:(BOOL)hasSteps
                        actionHandler:
                            (id<ConfirmationAlertActionHandler>)actionHandler
                            titleText:(NSString*)titleText {
  if ((self = [super init])) {
    self.actionHandler = actionHandler;
    _useDefaultAppsDestination = useDefaultAppsDestination;
    _useDefaultAppsDestination |= IsDefaultAppsDestinationAvailable() &&
                                  IsUseDefaultAppsDestinationForPromosEnabled();
    [self addVideoSection];
    [self addInformationSectionWithDismissButton:hasDismissButton
                                hasRemindMeLater:hasRemindMeLater
                       useDefaultAppsDestination:useDefaultAppsDestination
                                        hasSteps:hasSteps
                                   actionHandler:actionHandler
                                       titleText:titleText];
    [self.view setBackgroundColor:[UIColor colorNamed:kGrey100Color]];

    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(selectAnimationForCurrentStyle)];
  }
  return self;
}

#pragma mark - Private

// Adds the top part of the view which contains the video animation.
- (void)addVideoSection {
  self.animationViewWrapper = [self createAnimation:[self animationAssetName]];
  self.animationViewWrapper.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewAnimationViewId;
  self.animationViewWrapperDarkMode =
      [self createAnimation:[self animationAssetNameDarkmode]];
  self.animationViewWrapperDarkMode.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewDarkAnimationViewId;

  // Set the text localization.
  NSDictionary* textProvider = @{
    kBrowserAppKeypath :
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_BROWSER_APP),
    kDefaultBrowserAppKeypath : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    kChromeKeypath : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME)
  };
  [self.animationViewWrapper setDictionaryTextProvider:textProvider];
  [self.animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  [self.view addSubview:self.animationViewWrapper.animationView];
  [self.view addSubview:self.animationViewWrapperDarkMode.animationView];

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
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.animationViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.animationViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
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
  config.shouldLoop = YES;
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
                     useDefaultAppsDestination:(BOOL)useDefaultAppsDestination
                                      hasSteps:(BOOL)hasSteps
                                 actionHandler:
                                     (id<ConfirmationAlertActionHandler>)
                                         actionHandler
                                     titleText:(NSString*)titleText {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.actionHandler = actionHandler;
  if (!titleText) {
    alertScreen.titleString =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT);
  } else {
    alertScreen.titleString = titleText;
  }
  alertScreen.configuration.primaryActionString = l10n_util ::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  alertScreen.imageHasFixedSize = YES;
  alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  alertScreen.customSpacingBeforeImageIfNoNavigationBar = kSpacing;
  alertScreen.topAlignedLayout = YES;
  alertScreen.customSpacingAfterImage = kSpacing;
  alertScreen.customSpacing = kSpacing;

  // The view can have either instruction steps or subtitles.
  if (hasSteps) {
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

    alertScreen.underTitleView = instructionView;
    alertScreen.shouldFillInformationStack = YES;
  } else {
    if (useDefaultAppsDestination) {
      alertScreen.subtitleString = l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_APPS_SUBTITLE_TEXT);
    } else {
      alertScreen.subtitleString = l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SUBTITLE_TEXT);
    }
  }

  if (hasDismissButton) {
    alertScreen.configuration.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_SECONDARY_BUTTON_TEXT);
  }

  if (hasRemindMeLater) {
    alertScreen.configuration.tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_TERTIARY_BUTTON_TEXT);
  }
  [alertScreen reloadConfiguration];

  [self addChildViewController:alertScreen];
  [self.view addSubview:alertScreen.view];
  [alertScreen didMoveToParentViewController:self];

  // Layout the alert view to take up bottom half of the view.
  alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [alertScreen.view.topAnchor constraintEqualToAnchor:self.view.centerYAnchor
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

- (NSString*)animationAssetName {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      [[UIDevice currentDevice] userInterfaceIdiom] ==
          UIUserInterfaceIdiomPad) {
    return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlIpad
                               : kDefaultBrowserAnimationIpad;
  }

  if (_useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtl
                               : kDefaultBrowserDefaultAppsAnimation;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtl
                             : kDefaultBrowserAnimation;
}

- (NSString*)animationAssetNameDarkmode {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      [[UIDevice currentDevice] userInterfaceIdiom] ==
          UIUserInterfaceIdiomPad) {
    return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlDarkmodeIpad
                               : kDefaultBrowserAnimationDarkmodeIpad;
  }

  if (_useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtlDarkmode
                               : kDefaultBrowserDefaultAppsAnimationDarkmode;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlDarkmode
                             : kDefaultBrowserAnimationDarkmode;
}

@end
