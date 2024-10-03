// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_screenshot_view_controller.h"

#import "base/values.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kSpacingBeforeImageIfNoNavigationBar = 24;
constexpr CGFloat kLabelBottomMargin = -40;
constexpr CGFloat kLabelFontSize = 15;
NSString* const kDarkModeAnimationSuffix = @"_darkmode";
}  // namespace

@interface WhatsNewScreenshotViewController ()

// Custom screenshow view.
@property(nonatomic, strong) id<LottieAnimation> screenshotViewWrapper;
// Custom screenshow view in dark mode.
@property(nonatomic, strong) id<LottieAnimation> screenshotViewWrapperDarkMode;
// Child view controller used to display the alert full-screen.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;
// Label displayed indicating that the What's New feature is only available on
// iPhone.
@property(nonatomic, strong) UILabel* iPhoneOnlyLabel;
// What's New item.
@property(nonatomic, strong) WhatsNewItem* item;
// What's New command handler.
@property(nonatomic, weak) id<WhatsNewCommands> whatsNewHandler;
@end

@implementation WhatsNewScreenshotViewController

- (instancetype)initWithWhatsNewItem:(WhatsNewItem*)item
                     whatsNewHandler:(id<WhatsNewCommands>)whatsNewHandler {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _item = item;
    _screenshotViewWrapper = [self createAnimation:_item.screenshotName];
    _screenshotViewWrapperDarkMode = [self
        createAnimation:[_item.screenshotName
                            stringByAppendingString:kDarkModeAnimationSuffix]];

    // Set the text localization for the screenshot.
    [_screenshotViewWrapper
        setDictionaryTextProvider:_item.screenshotTextProvider];
    [_screenshotViewWrapperDarkMode
        setDictionaryTextProvider:_item.screenshotTextProvider];
    self.whatsNewHandler = whatsNewHandler;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismiss)];

  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];
  [self createConfirmationAlertScreen:self.item.title
                       subtitleString:self.item.subtitle
                  primaryActionString:self.item.primaryActionTitle
                secondaryActionString:
                    l10n_util::GetNSString(
                        IDS_IOS_WHATS_NEW_SHOW_INSTRUCTIONS_TITLE)];

  [self configureAnimationView];
  [self configureAlertScreen];
  if (self.item.isIphoneOnly &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [self configureLabelView];
  }
  [self layoutAlertScreen];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(toggleDarkModeOnTraitChange)];
  }
}

- (void)dismiss {
  [self.whatsNewHandler dismissWhatsNew];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self toggleDarkModeOnTraitChange];
}
#endif

#pragma mark - Private

// Creates a confirmation alert view controller and sets the strings.
- (void)createConfirmationAlertScreen:(NSString*)titleString
                       subtitleString:(NSString*)subtitleString
                  primaryActionString:(NSString*)primaryActionString
                secondaryActionString:(NSString*)secondaryActionString {
  DCHECK(!self.alertScreen);
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.titleString = titleString;
  alertScreen.subtitleString = subtitleString;
  if (!self.item.isIphoneOnly ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    alertScreen.primaryActionString = primaryActionString;
    alertScreen.secondaryActionString = secondaryActionString;
  }
  alertScreen.actionHandler = self.actionHandler;
  self.alertScreen = alertScreen;
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = 1000;
  return ios::provider::GenerateLottieAnimation(config);
}

// Configures the alertScreen view.
- (void)configureAlertScreen {
  DCHECK(self.alertScreen);
  self.alertScreen.imageHasFixedSize = YES;
  self.alertScreen.showDismissBarButton = NO;
  self.alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  self.alertScreen.customSpacingBeforeImageIfNoNavigationBar =
      kSpacingBeforeImageIfNoNavigationBar;
  self.alertScreen.topAlignedLayout = YES;

  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];

  [self.alertScreen didMoveToParentViewController:self];
}

// Configures the iPhoneOnlyLabel view.
- (void)configureLabelView {
  self.iPhoneOnlyLabel = [[UILabel alloc] init];
  self.iPhoneOnlyLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.iPhoneOnlyLabel.font = [UIFont systemFontOfSize:kLabelFontSize
                                                weight:UIFontWeightRegular];
  self.iPhoneOnlyLabel.text =
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_IPHONE_ONLY_TITLE);
  self.iPhoneOnlyLabel.textColor = [UIColor blackColor];
  self.iPhoneOnlyLabel.userInteractionEnabled = NO;
  [self.view addSubview:self.iPhoneOnlyLabel];

  [NSLayoutConstraint activateConstraints:@[
    [self.iPhoneOnlyLabel.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:kLabelBottomMargin],
    [self.iPhoneOnlyLabel.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

// Configures the animation view and its constraints.
- (void)configureAnimationView {
  [self.view addSubview:self.screenshotViewWrapper.animationView];
  [self.view addSubview:self.screenshotViewWrapperDarkMode.animationView];

  self.screenshotViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.screenshotViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  self.screenshotViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.screenshotViewWrapperDarkMode.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [self.screenshotViewWrapper.animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.screenshotViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.screenshotViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:29],
    [self.screenshotViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];

  AddSameConstraints(self.screenshotViewWrapperDarkMode.animationView,
                     self.screenshotViewWrapper.animationView);

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  self.screenshotViewWrapper.animationView.hidden = darkModeEnabled;
  self.screenshotViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
  [self updateAnimationsPlaying];
}

// Checks if the animations are hidden or unhidden and plays (or stops) them
// accordingly.
- (void)updateAnimationsPlaying {
  self.screenshotViewWrapper.animationView.hidden
      ? [self.screenshotViewWrapper stop]
      : [self.screenshotViewWrapper play];
  self.screenshotViewWrapperDarkMode.animationView.hidden
      ? [self.screenshotViewWrapperDarkMode stop]
      : [self.screenshotViewWrapperDarkMode play];
}

// Toggle dark mode view when UITraitUserInterfaceStyle is changed on device.
- (void)toggleDarkModeOnTraitChange {
  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  self.screenshotViewWrapper.animationView.hidden = darkModeEnabled;
  self.screenshotViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
}

@end
