// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bubble/public/in_product_help_type.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/fakebox_focuser.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_view.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

@interface NewTabPageHeaderViewController ()

// `YES` if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

@property(nonatomic, strong, readonly) NewTabPageHeaderView* headerView;

// Name of the default search engine. Used for the omnibox placeholder text.
@property(nonatomic, copy) NSString* defaultSearchEngineName;

@end

@implementation NewTabPageHeaderViewController {
  BOOL _useNewBadgeForLensButton;
  BOOL _useNewBadgeForCustomizationMenu;
  // Tracks if the mutator has already been notified of the Lens entrypoint
  // "new" badge display.
  BOOL _didNotifyLensBadgeDisplay;
  // Tracks if the mutator has already been notified of the Homepage
  // Customization "new" badge display.
  BOOL _didNotifyCustomizationBadgeDisplay;
}

- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
                 useNewBadgeForCustomizationMenu:
                     (BOOL)useNewBadgeForCustomizationMenu {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _useNewBadgeForLensButton = useNewBadgeForLensButton;
    _useNewBadgeForCustomizationMenu = useNewBadgeForCustomizationMenu;

  }
  return self;
}

- (void)loadView {
  self.view = [[NewTabPageHeaderView alloc]
      initWithUseNewBadgeForLensButton:_useNewBadgeForLensButton];
}

- (NewTabPageHeaderView*)headerView {
  return (NewTabPageHeaderView*)self.view;
}

#pragma mark - Public

- (UIView*)toolBarView {
  return self.headerView.toolBarView;
}

- (UIView*)fakeOmniboxView {
  if (IsComposeboxIOSEnabled()) {
    return self.headerView.fakeOmniboxContainer;
  }
  return self.headerView.omnibox;
}

- (void)expandHeaderForFocus {
  [self.headerView expandHeaderForFocus];
}

- (void)completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
    (UIViewAnimatingPosition)finalPosition {
  if (!IsComposeboxIOSEnabled()) {
    self.headerView.omnibox.hidden = YES;
    self.headerView.cancelButton.hidden = YES;
    self.headerView.searchHintLabel.alpha = 1;
    self.headerView.voiceSearchButton.alpha = 1;
  }
  if (finalPosition == UIViewAnimatingPositionEnd &&
      self.delegate.scrolledToMinimumHeight) {
    // Check to see if the collection are still scrolled to the top --
    // it's possible (and difficult) to unfocus the omnibox and initiate a
    // -shiftTilesDownForOmniboxDefocus before the animation here completes.
    if (IsSplitToolbarMode(self)) {
      [self.fakeboxFocuserHandler onFakeboxAnimationComplete];
    } else {
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
    }
  }
}

// TODO(crbug.com/40251610): Name animateScrollAnimation something more aligned
// to its true state indication. Why update the constraints only sometimes?
- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets
            animateScrollAnimation:(BOOL)animateScrollAnimation {
  [self.headerView updateFakeOmniboxForOffset:offset
                                  screenWidth:screenWidth
                               safeAreaInsets:safeAreaInsets
                       animateScrollAnimation:animateScrollAnimation];
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  [self.headerView updateFakeOmniboxForWidth:width];
}

- (void)layoutHeader {
  [self.headerView layoutHeader];
}

- (CGFloat)headerHeight {
  return [self.headerView headerHeight];
}

- (CGFloat)pinnedOffsetY {
  CGFloat offsetY = [self.headerView headerHeight];
  if ([self.delegate shouldPinFakeOmnibox]) {
    offsetY -= content_suggestions::FakeToolbarHeight();
  }

  return AlignValueToPixel(offsetY);
}
#pragma mark - Accessors & Mutators

- (void)setDelegate:(id<NewTabPageHeaderViewDelegate>)delegate {
  _delegate = delegate;
  if (self.isViewLoaded) {
    self.headerView.delegate = delegate;
  }
}

- (void)setShowing:(BOOL)showing {
  _showing = showing;
  if (self.isViewLoaded) {
    self.headerView.showing = showing;
  }
}

- (void)setIsGoogleDefaultSearchEngine:(BOOL)isGoogleDefaultSearchEngine {
  _isGoogleDefaultSearchEngine = isGoogleDefaultSearchEngine;
  if (self.viewLoaded) {
    [self.headerView
        setIsGoogleDefaultSearchEngine:isGoogleDefaultSearchEngine];
  }
}

- (void)setAllowFontScaleAnimation:(BOOL)allowFontScaleAnimation {
  _allowFontScaleAnimation = allowFontScaleAnimation;
  if (self.isViewLoaded) {
    self.headerView.allowFontScaleAnimation = allowFontScaleAnimation;
  }
}

- (void)setNTPShortcutsHandler:
    (id<NewTabPageShortcutsHandler>)NTPShortcutsHandler {
  _NTPShortcutsHandler = NTPShortcutsHandler;
  if (self.isViewLoaded) {
    self.headerView.NTPShortcutsHandler = NTPShortcutsHandler;
  }
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  if (self.isViewLoaded) {
    self.headerView.layoutGuideCenter = layoutGuideCenter;
  }
}

- (UIButton*)customizationMenuButton {
  return [self.headerView customizationMenuButton];
}

- (UIButton*)identityDiscButton {
  return self.headerView.identityDiscButton;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.headerView.layoutGuideCenter = self.layoutGuideCenter;
  self.headerView.commandHandler = self.commandHandler;
  self.headerView.toolbarDelegate = self.toolbarDelegate;
  self.headerView.delegate = self.delegate;

  self.headerView.NTPShortcutsHandler = self.NTPShortcutsHandler;
  [self.headerView
      setIsGoogleDefaultSearchEngine:self.isGoogleDefaultSearchEngine];
  self.headerView.showing = self.showing;

  [self.headerView setupSubviews];

  if (self.headerView.lensButton) {
    [self.layoutGuideCenter referenceView:self.headerView.lensButton
                                underName:kFakeboxLensIconGuide];
  }

  [self addCustomizationMenu];

  // Add a tools (overflow) menu entrypoint beside the customization menu.
  if (IsChromeNextIaEnabled()) {
    [self addToolsMenuIfNeeded];
  }

  self.headerView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if (_useNewBadgeForLensButton && !_didNotifyLensBadgeDisplay) {
    [self.mutator notifyLensBadgeDisplayed];
    _didNotifyLensBadgeDisplay = YES;
  }

  if (_useNewBadgeForCustomizationMenu &&
      !_didNotifyCustomizationBadgeDisplay) {
    [self.mutator notifyCustomizationBadgeDisplayed];
    _didNotifyCustomizationBadgeDisplay = YES;
  }

  // Check if the identity disc button was properly set before the view appears.
  DCHECK(self.identityDiscButton);
  DCHECK(self.identityDiscButton.accessibilityLabel);

  [self maybeShowSwitchAccountsIPH];
}

- (void)omniboxDidEndEditing {
  // Return early if the view is already showing.
  if (self.view.alpha == 1) {
    return;
  }
  self.view.alpha = 1;
}

- (void)hideBadgeOnCustomizationMenu {
  [self.headerView hideBadgeOnCustomizationMenu];
}

- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  self.headerView.tabGroupIndicatorView = view;
}

#pragma mark - UIContentContainer

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;

  void (^transitionBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext>) {
        [weakSelf updateLayoutForTraitCollection:newCollection];
      };

  [coordinator animateAlongsideTransition:transitionBlock completion:nil];
}

#pragma mark - FakeboxButtonsSnapshotProvider

- (UIView*)fakeboxButtonsSnapshot {
  return [self.headerView fakeboxButtonsSnapshot];
}

#pragma mark - Private

// Creates the Home customization menu and adds it to the header view.
- (void)addCustomizationMenu {
  UIButton* customizationMenuButton =
      [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero];

  if (!IsNTPBackgroundCustomizationEnabled()) {
    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kPencilSymbol, ntp_home::kNTPMenuButtonIconSize);
    [customizationMenuButton setImage:icon forState:UIControlStateNormal];
    customizationMenuButton.backgroundColor =
        [self defaultButtonBackgroundColor];

    UIColor* tintColor = [UIColor colorNamed:kBlue600Color];
    customizationMenuButton.tintColor = tintColor;

    customizationMenuButton.layer.cornerRadius =
        ntp_home::kNTPMenuButtonCornerRadius;
    customizationMenuButton.clipsToBounds = YES;
  }

  customizationMenuButton.accessibilityIdentifier =
      kNTPCustomizationMenuButtonIdentifier;
  customizationMenuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_ACCESSIBILITY_LABEL);

  [customizationMenuButton addTarget:self.commandHandler
                              action:@selector(customizationMenuWasTapped:)
                    forControlEvents:UIControlEventTouchUpInside];

  [self.headerView setCustomizationMenuButton:customizationMenuButton
                                 withNewBadge:_useNewBadgeForCustomizationMenu];
}

// Creates the Tools menu and adds it to the header view (iPhone only)
- (void)addToolsMenuIfNeeded {
  CHECK(IsChromeNextIaEnabled());

  // If the App Bar is not available (iPad), the Tools menu should not be added
  // to the header view.
  if (CanShowTabStrip(self)) {
    return;
  }

  UIButton* toolsMenuButton =
      [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero];

  if (!IsNTPBackgroundCustomizationEnabled()) {
    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kEllipsisSymbol, ntp_home::kNTPMenuButtonIconSize);
    [toolsMenuButton setImage:icon forState:UIControlStateNormal];
    toolsMenuButton.backgroundColor = [self defaultButtonBackgroundColor];

    UIColor* tintColor = [UIColor colorNamed:kBlue600Color];
    toolsMenuButton.tintColor = tintColor;
    toolsMenuButton.layer.cornerRadius = ntp_home::kNTPMenuButtonCornerRadius;
    toolsMenuButton.clipsToBounds = YES;
  }

  toolsMenuButton.accessibilityIdentifier = kNTPToolsMenuButtonIdentifier;
  toolsMenuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU);

  [toolsMenuButton addTarget:self.commandHandler
                      action:@selector(toolsMenuWasTapped:)
            forControlEvents:UIControlEventTouchUpInside];

  self.headerView.toolsMenuButton = toolsMenuButton;
}

// Helper for `-willTransitionToTraitCollection:withTransitionCoordinator:`.
// Updates the layout of the header according to the `traitCollection`.
- (void)updateLayoutForTraitCollection:(UITraitCollection*)traitCollection {
  if (IsChromeNextIaEnabled()) {
    [self.headerView resetSplitToolbarResizing];
    return;
  }

  BOOL isSplitToolbarMode = IsSplitToolbarMode(traitCollection);

  // Ensure omnibox is reset when not a regular tablet.
  if (isSplitToolbarMode && !CanShowTabStrip(traitCollection)) {
    [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
  }

  // Fake Tap button only needs to work in portrait. Disable the button
  // in landscape because in landscape the button covers logoView (which
  // need to handle taps).
  self.headerView.fakeTapButton.userInteractionEnabled = isSplitToolbarMode;
}

- (void)focusAccessibilityOnOmnibox {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.headerView.fakeOmniboxContainer);
}

- (void)maybeShowSwitchAccountsIPH {
  if (!self.headerView.isSignedIn) {
    return;
  }

  // Note: BubblePresenter will take care to only show the bubble if the page is
  // scrolled to the top.
  [self.helpHandler
      presentInProductHelpWithType:
          InProductHelpType::kSwitchAccountsWithNTPAccountParticleDisc];
}

// Returns the default background color for buttons based on the current
// appearance.
- (UIColor*)defaultButtonBackgroundColor {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                   ? [UIColor colorNamed:kTabGroupFaviconBackgroundColor]
                   : [[UIColor colorNamed:kSolidWhiteColor]
                         colorWithAlphaComponent:0.75];
      }];
}

@end
