// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/omnibox_event_global_tracker.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/utils.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/custom_highlight_button.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_view.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_view_delegate.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/toolbar_swipe_gesture_in_product_help_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns whether `view` could display and animate correctly within `guide`. If
// NO, elements in `view` may be hidden or overlap with each other during the
// animation.
BOOL CanGestureInProductHelpViewFitInGuide(GestureInProductHelpView* view,
                                           UILayoutGuide* guide) {
  CGSize guide_size = guide.layoutFrame.size;
  CGSize view_fitting_size =
      [view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  return view_fitting_size.width <= guide_size.width &&
         view_fitting_size.height <= guide_size.height;
}

}  // namespace

@interface BubblePresenter () <GestureInProductHelpViewDelegate>

// Used to display the bottom toolbar tip in-product help promotion bubble.
// `nil` if the tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that `userEngaged` remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* bottomToolbarTipBubblePresenter;
// Used to display the new tab tip in-product help promotion bubble. `nil` if
// the new tab tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that `userEngaged` remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* openNewTabIPHBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* sharePageIPHBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* tabGridIPHBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* discoverFeedHeaderMenuTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* readingListTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* followWhileBrowsingBubbleTipPresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* defaultPageModeTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* whatsNewBubblePresenter;
@property(nonatomic, strong) BubbleViewControllerPresenter*
    priceNotificationsWhileBrowsingBubbleTipPresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* lensKeyboardPresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* parcelTrackingTipBubblePresenter;
@property(nonatomic, strong) GestureInProductHelpView* pullToRefreshGestureIPH;
@property(nonatomic, strong)
    GestureInProductHelpView* swipeBackForwardGestureIPH;
@property(nonatomic, strong)
    ToolbarSwipeGestureInProductHelpView* toolbarSwipeGestureIPH;
@property(nonatomic, assign) WebStateList* webStateList;
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;
@property(nonatomic, assign) HostContentSettingsMap* settingsMap;
// Whether the presenter is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

@end

@implementation BubblePresenter {
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;

  raw_ptr<PrefService> _prefService;

  id<TabStripCommands> _tabStripCommandsHandler;
}

#pragma mark - Public

- (instancetype)
    initWithDeviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)
            deviceSwitcherResultDispatcher
                    hostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                               prefService:(PrefService*)prefService
                   tabStripCommandsHandler:
                       (id<TabStripCommands>)tabStripCommandsHandler
                                   tracker:(feature_engagement::Tracker*)
                                               engagementTracker
                              webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(prefService);
    DCHECK(webStateList);

    _webStateList = webStateList;
    _engagementTracker = engagementTracker;
    _settingsMap = settingsMap;
    _deviceSwitcherResultDispatcher = deviceSwitcherResultDispatcher;
    _prefService = prefService;
    _tabStripCommandsHandler = tabStripCommandsHandler;
    self.started = YES;
  }
  return self;
}

- (void)stop {
  [self hideAllHelpBubbles];
  self.started = NO;
  self.webStateList = nullptr;
  self.engagementTracker = nullptr;
  self.settingsMap = nullptr;
}

- (void)hideAllHelpBubbles {
  [self.sharePageIPHBubblePresenter dismissAnimated:NO];
  [self.openNewTabIPHBubblePresenter dismissAnimated:NO];
  [self.tabGridIPHBubblePresenter dismissAnimated:NO];
  [self.bottomToolbarTipBubblePresenter dismissAnimated:NO];
  [self.discoverFeedHeaderMenuTipBubblePresenter dismissAnimated:NO];
  [self.readingListTipBubblePresenter dismissAnimated:NO];
  [self.followWhileBrowsingBubbleTipPresenter dismissAnimated:NO];
  [self.priceNotificationsWhileBrowsingBubbleTipPresenter dismissAnimated:NO];
  [self.whatsNewBubblePresenter dismissAnimated:NO];
  [self.lensKeyboardPresenter dismissAnimated:NO];
  [self.defaultPageModeTipBubblePresenter dismissAnimated:NO];
  [self.parcelTrackingTipBubblePresenter dismissAnimated:NO];
  [self hideAllGestureInProductHelpViewsForReason:IPHDismissalReasonType::
                                                      kUnknown];
}

- (void)handleTapOutsideOfVisibleGestureInProductHelp {
  [self hideAllGestureInProductHelpViewsForReason:
            IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView];
}

- (void)presentShareButtonHelpBubbleIfEligible {
  if (!iph_for_new_chrome_user::IsUserNewSafariSwitcher(
          _deviceSwitcherResultDispatcher)) {
    return;
  }

  UIView* shareButtonView =
      [_layoutGuideCenter referencedViewUnderName:kShareButtonGuide];
  // Do not present if the share button is not visible.
  if (!shareButtonView || shareButtonView.hidden) {
    return;
  }

  // DCHECK if the type is not `CustomHighlightableButton`.
  __weak CustomHighlightableButton* shareButton =
      base::apple::ObjCCastStrict<CustomHighlightableButton>(shareButtonView);

  // Do not present if button is disabled.
  if (![shareButton isEnabled]) {
    return;
  }

  if (![self canPresentBubbleWithCheckTabScrolledToTop:NO]) {
    return;
  }

  BOOL isBottomOmnibox = IsBottomOmniboxSteadyStateEnabled() &&
                         _prefService->GetBoolean(prefs::kBottomOmnibox);
  BubbleArrowDirection arrowDirection =
      isBottomOmnibox ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_SHARE_THIS_PAGE_IPH_TEXT);
  NSString* announcement =
      l10n_util::GetNSString(IDS_IOS_SHARE_THIS_PAGE_IPH_ANNOUNCEMENT);
  CGPoint shareButtonAnchor = [self anchorPointToGuide:kShareButtonGuide
                                             direction:arrowDirection];

  auto presentAction = ^() {
    [shareButton setCustomHighlighted:YES];
  };
  auto dismissAction = ^() {
    [shareButton setCustomHighlighted:NO];
  };

  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHiOSShareToolbarItemFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentBottomOrTrailing
                         text:text
        voiceOverAnnouncement:announcement
                  anchorPoint:shareButtonAnchor
                presentAction:presentAction
                dismissAction:dismissAction];
  if (!presenter) {
    return;
  }

  self.sharePageIPHBubblePresenter = presenter;
}

- (void)presentDiscoverFeedHeaderTipBubble {
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_DISCOVER_FEED_HEADER_IPH);

  UIView* menuButton = [self.layoutGuideCenter
      referencedViewUnderName:kFeedHeaderManagementButtonGuide];
  // Checks "canPresentBubble" after checking that the NTP with feed is visible.
  // This ensures that the feature tracker doesn't trigger the IPH event if the
  // bubble isn't shown, which would prevent it from ever being shown again.
  if (!menuButton || ![self canPresentBubble]) {
    return;
  }
  CGPoint discoverFeedHeaderAnchor =
      [menuButton.superview convertPoint:menuButton.frame.origin toView:nil];
  // Anchor the IPH 1/3 of the way through the button. Anchoring it midway
  // doesn't work since the button is too close to the edge, which would cause
  // the bubble to bleed out the screen.
  discoverFeedHeaderAnchor.x += menuButton.frame.size.width / 3;

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `discoverFeedHeaderMenuTipBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHDiscoverFeedHeaderFeature
                    direction:arrowDirection
                         text:text
        voiceOverAnnouncement:nil
                  anchorPoint:discoverFeedHeaderAnchor];
  if (!presenter)
    return;

  self.discoverFeedHeaderMenuTipBubblePresenter = presenter;
}

- (void)presentFollowWhileBrowsingTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_FOLLOW_WHILE_BROWSING_IPH);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `followWhileBrowsingBubbleTipPresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHFollowWhileBrowsingFeature
                    direction:arrowDirection
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(
                                  IDS_IOS_FOLLOW_WHILE_BROWSING_IPH)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.followWhileBrowsingBubbleTipPresenter = presenter;
}

- (void)presentDefaultSiteViewTipBubble {
  if (![self canPresentBubble])
    return;
  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  if (!currentWebState ||
      ShouldLoadUrlInDesktopMode(currentWebState->GetVisibleURL(),
                                 self.settingsMap)) {
    return;
  }

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_TIP);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing presenter to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHDefaultSiteViewFeature
                    direction:arrowDirection
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(
                                  IDS_IOS_DEFAULT_PAGE_MODE_TIP_VOICE_OVER)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.defaultPageModeTipBubblePresenter = presenter;
}

- (void)presentWhatsNewBottomToolbarBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_WHATS_NEW_IPH_TEXT);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `whatsNewBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHWhatsNewFeature
                    direction:arrowDirection
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(IDS_IOS_WHATS_NEW_IPH_TEXT)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.whatsNewBubblePresenter = presenter;
}

- (void)presentPriceNotificationsWhileBrowsingTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TOAST_IPH_TEXT);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `whatsNewBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:
                feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature
                          direction:arrowDirection
                               text:text
              voiceOverAnnouncement:text
                        anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.priceNotificationsWhileBrowsingBubbleTipPresenter = presenter;
}

- (void)presentLensKeyboardTipBubble {
  if (![self canPresentBubbleWithCheckTabScrolledToTop:NO]) {
    return;
  }

  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text = l10n_util::GetNSString(IDS_IOS_LENS_KEYBOARD_IPH_TEXT);
  CGPoint lensButtonAnchor = [self anchorPointToGuide:kLensKeyboardButtonGuide
                                            direction:arrowDirection];

  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHiOSLensKeyboardFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTopOrLeading
                         text:text
        voiceOverAnnouncement:text
                  anchorPoint:lensButtonAnchor
                presentAction:nil
                dismissAction:nil];
  if (!presenter) {
    return;
  }

  self.lensKeyboardPresenter = presenter;
}

- (void)presentParcelTrackingTipBubble {
  if (![self canPresentBubble]) {
    return;
  }

  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text = l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_IPH);

  CGPoint magicStackAnchor = [self anchorPointToGuide:kMagicStackGuide
                                            direction:arrowDirection];

  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHiOSParcelTrackingFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentCenter
                         text:text
        voiceOverAnnouncement:text
                  anchorPoint:magicStackAnchor
                presentAction:nil
                dismissAction:nil];

  if (!presenter) {
    return;
  }

  self.parcelTrackingTipBubblePresenter = presenter;
}

- (void)presentNewTabToolbarItemBubble {
  if (!iph_for_new_chrome_user::IsUserNewSafariSwitcher(
          _deviceSwitcherResultDispatcher)) {
    return;
  }

  UIView* newTabToolbarView =
      [_layoutGuideCenter referencedViewUnderName:kNewTabButtonGuide];
  // Do not present if the new tab button is not visible.
  if (!newTabToolbarView || newTabToolbarView.hidden) {
    return;
  }

  if (![self canPresentBubbleWithCheckTabScrolledToTop:NO]) {
    return;
  }

  // Do not present the new tab IPH on NTP.
  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  if (!currentWebState || IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_OPEN_NEW_TAB_IPH_TEXT);
  std::u16string newTabButtonA11yLabel = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_TAB));
  NSString* announcement = l10n_util::GetNSStringF(
      IDS_IOS_OPEN_NEW_TAB_IPH_ANNOUNCEMENT, newTabButtonA11yLabel);
  CGPoint newTabButtonAnchor = [self anchorPointToGuide:kNewTabButtonGuide
                                              direction:arrowDirection];

  __weak id<ToolbarCommands> weakToolbarCommandsHandler =
      _toolbarCommandsHandler;
  __weak id<TabStripCommands> weakTabStripCommandsHandler =
      _tabStripCommandsHandler;

  // TODO(crbug.com/1439920): refactor to use CustomHighlightableButton API.
  ProceduralBlock presentAction = ^{
    [weakTabStripCommandsHandler setNewTabButtonOnTabStripIPHHighlighted:YES];
    [weakToolbarCommandsHandler setNewTabButtonIPHHighlighted:YES];
  };
  ProceduralBlock dismissAction = ^{
    [weakTabStripCommandsHandler setNewTabButtonOnTabStripIPHHighlighted:NO];
    [weakToolbarCommandsHandler setNewTabButtonIPHHighlighted:NO];
  };

  // If the feature engagement tracker does not consider it valid to display
  // the new tab tip, then end early to prevent the potential reassignment
  // of the existing `openNewTabIPHBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:feature_engagement::
                                        kIPHiOSNewTabToolbarItemFeature
                          direction:arrowDirection
                          alignment:BubbleAlignmentBottomOrTrailing
                               text:text
              voiceOverAnnouncement:announcement
                        anchorPoint:newTabButtonAnchor
                      presentAction:presentAction
                      dismissAction:dismissAction];
  if (!presenter) {
    return;
  }

  self.openNewTabIPHBubblePresenter = presenter;
}

// Optionally presents a bubble associated with the tab grid iph. If the feature
// engagement tracker determines it is valid to show the new tab tip, then it
// initializes `tabGridIPHBubblePresenter` and presents the bubble. If it is
// not valid to show the new tab tip, `tabGridIPHBubblePresenter` is set to
// `nil` and no bubble is shown. This method requires that `self.browserState`
// is not NULL.
- (void)presentTabGridToolbarItemBubble {
  if (!iph_for_new_chrome_user::IsUserNewSafariSwitcher(
          _deviceSwitcherResultDispatcher)) {
    return;
  }

  UIView* tabGridToolbarView =
      [_layoutGuideCenter referencedViewUnderName:kNewTabButtonGuide];
  // Do not present if the tab grid button is not visible.
  if (!tabGridToolbarView || tabGridToolbarView.hidden) {
    return;
  }

  if (![self canPresentBubbleWithCheckTabScrolledToTop:NO]) {
    return;
  }

  // only present the IPH when tab count > 1.
  if (self.webStateList->count() <= 1) {
    return;
  }

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_SEE_ALL_OPEN_TABS_IPH_TEXT);
  NSString* announcement =
      l10n_util::GetNSString(IDS_IOS_SEE_ALL_OPEN_TABS_IPH_ANNOUNCEMENT);
  CGPoint tabGridButtonAnchor = [self anchorPointToGuide:kTabSwitcherGuide
                                               direction:arrowDirection];

  __weak id<ToolbarCommands> weakToolbarCommandsHandler =
      _toolbarCommandsHandler;
  // TODO(crbug.com/1439920): refactor to use CustomHighlightableButton API.
  auto presentAction = ^() {
    [weakToolbarCommandsHandler setTabGridButtonIPHHighlighted:YES];
  };
  auto dismissAction = ^() {
    [weakToolbarCommandsHandler setTabGridButtonIPHHighlighted:NO];
  };

  // If the feature engagement tracker does not consider it valid to display
  // the new tab tip, then end early to prevent the potential reassignment
  // of the existing `tabGridIPHBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:feature_engagement::
                                        kIPHiOSTabGridToolbarItemFeature
                          direction:arrowDirection
                          alignment:BubbleAlignmentBottomOrTrailing
                               text:text
              voiceOverAnnouncement:announcement
                        anchorPoint:tabGridButtonAnchor
                      presentAction:presentAction
                      dismissAction:dismissAction];
  if (!presenter) {
    return;
  }

  self.tabGridIPHBubblePresenter = presenter;
}

- (void)presentPullToRefreshGestureInProductHelp {
  if (UIAccessibilityIsVoiceOverRunning() || (![self canPresentBubble])) {
    // TODO(crbug.com/1521489): Add voice over announcement once fixed.
    return;
  }
  const base::Feature& pullToRefreshFeature =
      feature_engagement::kIPHiOSPullToRefreshFeature;
  BOOL userEligibleForPullToRefreshIPH =
      iph_for_new_chrome_user::IsUserNewSafariSwitcher(
          _deviceSwitcherResultDispatcher) &&
      self.engagementTracker->WouldTriggerHelpUI(pullToRefreshFeature);
  if (!userEligibleForPullToRefreshIPH) {
    return;
  }
  NSString* text = l10n_util::GetNSString(IDS_IOS_PULL_TO_REFRESH_IPH);
  self.pullToRefreshGestureIPH =
      [self presentGestureInProductHelpForFeature:pullToRefreshFeature
                                   swipeDirection:
                                       UISwipeGestureRecognizerDirectionDown
                                             text:text];
  [self.pullToRefreshGestureIPH startAnimation];
}

- (void)presentBackForwardSwipeGestureInProductHelp {
  if (UIAccessibilityIsVoiceOverRunning() ||
      (![self canPresentBubbleWithCheckTabScrolledToTop:NO])) {
    return;
  }
  const base::Feature& backForwardSwipeFeature =
      feature_engagement::kIPHiOSSwipeBackForwardFeature;
  BOOL userEligible =
      IsFirstRunRecent(base::Days(60)) &&
      self.engagementTracker->WouldTriggerHelpUI(backForwardSwipeFeature);
  if (!userEligible) {
    return;
  }

  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  if (IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }

  // Retrieve swipe-able directions.
  const web::NavigationManager* navigationManager =
      currentWebState->GetNavigationManager();
  BOOL back = navigationManager->CanGoBack();
  BOOL forward = navigationManager->CanGoForward();
  int textId = IDS_IOS_BACK_FORWARD_SWIPE_IPH_BACK_ONLY;
  if (forward) {
    textId = back ? IDS_IOS_BACK_FORWARD_SWIPE_IPH
                  : IDS_IOS_BACK_FORWARD_SWIPE_IPH_FORWARD_ONLY;
  }

  UISwipeGestureRecognizerDirection direction =
      back ^ UseRTLLayout() ? UISwipeGestureRecognizerDirectionRight
                            : UISwipeGestureRecognizerDirectionLeft;
  self.swipeBackForwardGestureIPH = [self
      presentGestureInProductHelpForFeature:backForwardSwipeFeature
                             swipeDirection:direction
                                       text:l10n_util::GetNSString(textId)];
  self.swipeBackForwardGestureIPH.edgeSwipe = YES;
  if (back && forward) {
    self.swipeBackForwardGestureIPH.animationRepeatCount = 4;
    self.swipeBackForwardGestureIPH.bidirectional = YES;
  }
  [self.swipeBackForwardGestureIPH startAnimation];
}

- (void)presentToolbarSwipeGestureInProductHelp {
  // Inapplicable on iPad.
  if (ui::GetDeviceFormFactor() !=
          ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE ||
      UIAccessibilityIsVoiceOverRunning() ||
      (![self canPresentBubbleWithCheckTabScrolledToTop:NO])) {
    return;
  }
  const base::Feature& feature =
      feature_engagement::kIPHiOSSwipeToolbarToChangeTabFeature;
  BOOL userEligible = IsFirstRunRecent(base::Days(60)) &&
                      self.engagementTracker->WouldTriggerHelpUI(feature);
  if (!userEligible) {
    return;
  }
  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  if (IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }
  // Setup view constraints.
  NamedGuide* contentAreaGuide =
      [NamedGuide guideWithName:kContentAreaGuide
                           view:self.rootViewController.view];
  if (!contentAreaGuide) {
    return;
  }
  UILayoutGuide* guide = [[UILayoutGuide alloc] init];
  [self.rootViewController.view addLayoutGuide:guide];
  AddSameConstraintsToSides(
      guide, contentAreaGuide,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  NSLayoutConstraint* topConstraintForBottomEdgeSwipe = [guide.topAnchor
      constraintEqualToAnchor:self.rootViewController.view.topAnchor];
  NSLayoutConstraint* topConstraintForTopEdgeSwipe =
      [guide.topAnchor constraintEqualToAnchor:contentAreaGuide.topAnchor];
  NSLayoutConstraint* initialTopConstraint =
      self.rootViewController.traitCollection.verticalSizeClass ==
              UIUserInterfaceSizeClassRegular
          ? topConstraintForBottomEdgeSwipe
          : topConstraintForTopEdgeSwipe;
  initialTopConstraint.active = YES;

  // Check index to determine which directions are supported.
  int activeIndex = self.webStateList->active_index();
  // Configure IPH view.
  ToolbarSwipeGestureInProductHelpView* toolbarSwipeGestureIPH =
      [[ToolbarSwipeGestureInProductHelpView alloc]
          initWithBubbleBoundingSize:guide.layoutFrame.size
                           canGoBack:activeIndex > 0
                             forward:activeIndex <
                                     self.webStateList->count() - 1];
  [toolbarSwipeGestureIPH setTranslatesAutoresizingMaskIntoConstraints:NO];
  if (!CanGestureInProductHelpViewFitInGuide(toolbarSwipeGestureIPH, guide) ||
      !self.engagementTracker->ShouldTriggerHelpUI(feature)) {
    return;
  }
  toolbarSwipeGestureIPH.topConstraintForBottomEdgeSwipe =
      topConstraintForBottomEdgeSwipe;
  toolbarSwipeGestureIPH.topConstraintForTopEdgeSwipe =
      topConstraintForTopEdgeSwipe;
  [self.rootViewController.view addSubview:toolbarSwipeGestureIPH];
  AddSameConstraints(toolbarSwipeGestureIPH, guide);

  [toolbarSwipeGestureIPH startAnimation];
  self.toolbarSwipeGestureIPH = toolbarSwipeGestureIPH;
}

- (void)handleToolbarSwipeGesture {
  [self.toolbarSwipeGestureIPH
      dismissWithReason:IPHDismissalReasonType::
                            kSwipedAsInstructedByGestureIPH];
}

#pragma mark - GestureInProductHelpViewDelegate

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
            didDismissWithReason:(IPHDismissalReasonType)reason {
  const feature_engagement::Tracker::SnoozeAction snoozeAction =
      feature_engagement::Tracker::SnoozeAction::DISMISSED;
  if (view == self.pullToRefreshGestureIPH) {
    [self featureDismissed:feature_engagement::kIPHiOSPullToRefreshFeature
                withSnooze:snoozeAction];
  } else if (view == self.swipeBackForwardGestureIPH) {
    [self featureDismissed:feature_engagement::kIPHiOSSwipeBackForwardFeature
                withSnooze:snoozeAction];
  } else if (view == self.toolbarSwipeGestureIPH) {
    [self featureDismissed:feature_engagement::
                               kIPHiOSSwipeToolbarToChangeTabFeature
                withSnooze:snoozeAction];
  } else {
    NOTREACHED();
  }
}

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
    shouldHandleSwipeInDirection:(UISwipeGestureRecognizerDirection)direction {
  if (view == self.pullToRefreshGestureIPH) {
    [self.delegate bubblePresenterDidPerformPullToRefreshGesture:self];
  } else if (view == self.swipeBackForwardGestureIPH) {
    [self.delegate bubblePresenter:self
        didPerformSwipeToNavigateInDirection:direction];
  } else if (view == self.toolbarSwipeGestureIPH) {
    // Do nothing. Swipe happens outside of the view.
  } else {
    NOTREACHED();
  }
}

#pragma mark - Private

// Convenience method that calls -presentBubbleForFeature with default param
// values for `alignment`, `presentAction`, and `dismissAction`.
- (BubbleViewControllerPresenter*)
    presentBubbleForFeature:(const base::Feature&)feature
                  direction:(BubbleArrowDirection)direction
                       text:(NSString*)text
      voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
                anchorPoint:(CGPoint)anchorPoint {
  return [self presentBubbleForFeature:feature
                             direction:direction
                             alignment:BubbleAlignmentBottomOrTrailing
                                  text:text
                 voiceOverAnnouncement:voiceOverAnnouncement
                           anchorPoint:anchorPoint
                         presentAction:nil
                         dismissAction:nil];
}

// Presents and returns a bubble view controller for the `feature` with an arrow
// `direction`, an arrow `alignment` and a `text` on an `anchorPoint`.
- (BubbleViewControllerPresenter*)
    presentBubbleForFeature:(const base::Feature&)feature
                  direction:(BubbleArrowDirection)direction
                  alignment:(BubbleAlignment)alignment
                       text:(NSString*)text
      voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
                anchorPoint:(CGPoint)anchorPoint
              presentAction:(ProceduralBlock)presentAction
              dismissAction:(ProceduralBlock)dismissAction {
  DCHECK(self.engagementTracker);
  BubbleViewControllerPresenter* presenter =
      [self bubblePresenterForFeature:feature
                            direction:direction
                            alignment:alignment
                                 text:text
                        dismissAction:dismissAction];
  if (!presenter) {
    return nil;
  }
  presenter.voiceOverAnnouncement = voiceOverAnnouncement;
  if ([presenter canPresentInView:self.rootViewController.view
                      anchorPoint:anchorPoint] &&
      ([self shouldForcePresentBubbleForFeature:feature] ||
       self.engagementTracker->ShouldTriggerHelpUI(feature))) {
    [presenter presentInViewController:self.rootViewController
                                  view:self.rootViewController.view
                           anchorPoint:anchorPoint];
    if (presentAction) {
      presentAction();
    }
  }
  return presenter;
}

// If any gesture IPH visible, remove it and log the `reason` why it should be
// removed on UMA. Otherwise, do nothing. The presenter of any gesture IPH
// should make sure it's called when the user leaves the refreshed website,
// especially while the IPH is still visible.
- (void)hideAllGestureInProductHelpViewsForReason:
    (IPHDismissalReasonType)reason {
  [self.pullToRefreshGestureIPH dismissWithReason:reason];
  [self.swipeBackForwardGestureIPH dismissWithReason:reason];
  [self.toolbarSwipeGestureIPH dismissWithReason:reason];
}

#pragma mark - Private Utils

// Returns the anchor point for a bubble with an `arrowDirection` pointing to a
// `guideName`. The point is in the window coordinates.
- (CGPoint)anchorPointToGuide:(GuideName*)guideName
                    direction:(BubbleArrowDirection)arrowDirection {
  UILayoutGuide* guide =
      [self.layoutGuideCenter makeLayoutGuideNamed:guideName];
  DCHECK(guide);
  [self.rootViewController.view addLayoutGuide:guide];
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(guide.layoutFrame, arrowDirection);
  CGPoint anchorPointInWindow =
      [guide.owningView convertPoint:anchorPoint
                              toView:guide.owningView.window];
  [self.rootViewController.view removeLayoutGuide:guide];
  return anchorPointInWindow;
}

// Returns whether the tab can present a bubble tip.
// TODO(crbug.com/1448656): make most callsites pass NO for
// `CheckTabScrolledToTop` as it's error-prone.
- (BOOL)canPresentBubble {
  return [self canPresentBubbleWithCheckTabScrolledToTop:YES];
}

// Returns whether the tab can present a bubble tip. Whether tab being scrolled
// to top is required for presenting the bubble tip is determined by
// `checkTabScrolledToTop`.
- (BOOL)canPresentBubbleWithCheckTabScrolledToTop:(BOOL)checkTabScrolledToTop {
  // If BubblePresenter has been stopped, do not present the bubble.
  if (!self.started) {
    return NO;
  }
  // If the BVC is not visible, do not present the bubble.
  if (![self.delegate rootViewVisibleForBubblePresenter:self]) {
    return NO;
  }
  // Do not present the bubble if there is no current tab.
  if (!self.webStateList->GetActiveWebState()) {
    return NO;
  }
  // Do not present the bubble if the tab is not scrolled to the top.
  if (checkTabScrolledToTop && ![self isTabScrolledToTop]) {
    return NO;
  }
  return YES;
}

- (BOOL)isTabScrolledToTop {
  // If NTP exists, check if it is scrolled to top.
  if ([self.delegate isNTPActiveForBubblePresenter:self]) {
    return [self.delegate isNTPScrolledToTopForBubblePresenter:self];
  }
  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  CRWWebViewScrollViewProxy* scrollProxy =
      currentWebState->GetWebViewProxy().scrollViewProxy;
  CGPoint scrollOffset = scrollProxy.contentOffset;
  UIEdgeInsets contentInset = scrollProxy.contentInset;
  return AreCGFloatsEqual(scrollOffset.y, -contentInset.top);
}

// Returns a bubble associated with an in-product help promotion if
// it is valid to show the promotion and `nil` otherwise. `feature` is the
// base::Feature object associated with the given promotion. `direction` is the
// direction the bubble's arrow is pointing. `alignment` is the alignment of the
// arrow on the button. `text` is the text displayed by the bubble.
- (BubbleViewControllerPresenter*)
    bubblePresenterForFeature:(const base::Feature&)feature
                    direction:(BubbleArrowDirection)direction
                    alignment:(BubbleAlignment)alignment
                         text:(NSString*)text
                dismissAction:(ProceduralBlock)dismissAction {
  DCHECK(self.engagementTracker);
  // Capture `weakSelf` instead of the feature engagement tracker object
  // because `weakSelf` will safely become `nil` if it is deallocated, whereas
  // the feature engagement tracker will remain pointing to invalid memory if
  // its owner (the ChromeBrowserState) is deallocated.
  __weak BubblePresenter* weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallbackWithSnoozeAction =
      ^(IPHDismissalReasonType IPHDismissalReasonType,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        if (dismissAction) {
          dismissAction();
        }
        [weakSelf featureDismissed:feature withSnooze:snoozeAction];
      };

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:direction
                          alignment:alignment
               isLongDurationBubble:[self isLongDurationBubble:feature]
                  dismissalCallback:dismissalCallbackWithSnoozeAction];

  return bubbleViewControllerPresenter;
}

// If an in-product help message should be shown for `feature`, presents an IPH
// view covering the content area and return the view, otherwise return `nil`
// and do nothing. `direction` is the direction the bubble's arrow is pointing.
// `text` is the text displayed by the bubble.
//
// Note that this method does NOT start the animation. The caller should start
// the animation of the returned `GestureInProductHelpView` accordingly. This
// allows the caller to make modifications to the view before animating.
- (GestureInProductHelpView*)
    presentGestureInProductHelpForFeature:(const base::Feature&)feature
                           swipeDirection:
                               (UISwipeGestureRecognizerDirection)direction
                                     text:(NSString*)text {
  DCHECK(self.engagementTracker);
  NamedGuide* contentAreaGuide =
      [NamedGuide guideWithName:kContentAreaGuide
                           view:self.rootViewController.view];
  if (!contentAreaGuide) {
    return nil;
  }
  UILayoutGuide* boundingSizeGuide = [[UILayoutGuide alloc] init];
  UILayoutGuide* safeAreaGuide =
      self.rootViewController.view.safeAreaLayoutGuide;
  [self.rootViewController.view addLayoutGuide:boundingSizeGuide];

  BOOL isDirectionLeading = direction == UseRTLLayout()
                                ? UISwipeGestureRecognizerDirectionRight
                                : UISwipeGestureRecognizerDirectionLeft;
  switch (direction) {
    case UISwipeGestureRecognizerDirectionUp:
      AddSameConstraintsToSides(
          boundingSizeGuide, contentAreaGuide,
          LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);
      AddSameConstraintsToSides(boundingSizeGuide, safeAreaGuide,
                                LayoutSides::kBottom);
      break;
    case UISwipeGestureRecognizerDirectionDown:
      AddSameConstraintsToSides(boundingSizeGuide, contentAreaGuide,
                                LayoutSides::kLeading | LayoutSides::kTrailing |
                                    LayoutSides::kBottom);
      AddSameConstraintsToSides(boundingSizeGuide, safeAreaGuide,
                                LayoutSides::kTop);
      break;
    case UISwipeGestureRecognizerDirectionLeft:
    case UISwipeGestureRecognizerDirectionRight:
      if (isDirectionLeading) {
        AddSameConstraintsToSides(
            boundingSizeGuide, contentAreaGuide,
            LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kLeading);
        AddSameConstraintsToSides(boundingSizeGuide, safeAreaGuide,
                                  LayoutSides::kTrailing);
      } else {
        AddSameConstraintsToSides(
            boundingSizeGuide, contentAreaGuide,
            LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kTrailing);
        AddSameConstraintsToSides(boundingSizeGuide, safeAreaGuide,
                                  LayoutSides::kLeading);
      }
      break;
  }
  GestureInProductHelpView* gestureIPHView = [[GestureInProductHelpView alloc]
            initWithText:text
      bubbleBoundingSize:boundingSizeGuide.layoutFrame.size
          swipeDirection:direction];
  [gestureIPHView setTranslatesAutoresizingMaskIntoConstraints:NO];
  if (CanGestureInProductHelpViewFitInGuide(gestureIPHView,
                                            boundingSizeGuide) &&
      self.engagementTracker->ShouldTriggerHelpUI(feature)) {
    [self.rootViewController.view addSubview:gestureIPHView];
    gestureIPHView.delegate = self;
    AddSameConstraints(gestureIPHView, contentAreaGuide);
    return gestureIPHView;
  }
  return nil;
}

- (void)featureDismissed:(const base::Feature&)feature
              withSnooze:
                  (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  if (!self.engagementTracker) {
    return;
  }
  self.engagementTracker->DismissedWithSnooze(feature, snoozeAction);
}

// Returns YES if the bubble for `feature` has a long duration.
- (BOOL)isLongDurationBubble:(const base::Feature&)feature {
  // Display follow iph bubble with long duration.
  return feature.name ==
         feature_engagement::kIPHFollowWhileBrowsingFeature.name;
}

// Return YES if the bubble should always be presented. Ex. if force present
// bubble set by system experimental settings.
- (BOOL)shouldForcePresentBubbleForFeature:(const base::Feature&)feature {
  // Always present follow IPH if it's triggered by system experimental
  // settings.
  if (feature.name == feature_engagement::kIPHFollowWhileBrowsingFeature.name &&
      experimental_flags::ShouldAlwaysShowFollowIPH()) {
    return YES;
  }

  return NO;
}

@end
