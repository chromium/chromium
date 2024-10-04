// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter.h"

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
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_delegate.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_delegate.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/toolbar_swipe_gesture_in_product_help_view.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/utils.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
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

@interface BubblePresenter () <GestureInProductHelpViewDelegate,
                               OverlayPresenterObserving>

@end

@implementation BubblePresenter {
  // Required dependencies.
  LayoutGuideCenter* _layoutGuideCenter;
  raw_ptr<WebStateList> _webStateList;
  raw_ptr<feature_engagement::Tracker> _engagementTracker;

  // Overlay observing.
  raw_ptr<OverlayPresenter> _webContentOverlayPresenter;
  raw_ptr<OverlayPresenter> _infobarBannerPresenter;
  raw_ptr<OverlayPresenter> _infobarModalPresenter;
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;

  // Whether the presenter is started.
  BOOL _started;

  // List of existing bubble view presenters.
  BubbleViewControllerPresenter* _bottomToolbarTipBubblePresenter;
  BubbleViewControllerPresenter* _discoverFeedHeaderMenuTipBubblePresenter;
  BubbleViewControllerPresenter* _homeCustomizationMenuTipBubblePresenter;
  BubbleViewControllerPresenter* _readingListTipBubblePresenter;
  BubbleViewControllerPresenter* _followWhileBrowsingBubbleTipPresenter;
  BubbleViewControllerPresenter* _defaultPageModeTipBubblePresenter;
  BubbleViewControllerPresenter* _whatsNewBubblePresenter;
  BubbleViewControllerPresenter*
      _priceNotificationsWhileBrowsingBubbleTipPresenter;
  BubbleViewControllerPresenter* _lensKeyboardPresenter;
  BubbleViewControllerPresenter* _parcelTrackingTipBubblePresenter;
  BubbleViewControllerPresenter* _lensOverlayEntrypointBubblePresenter;

  // List of existing gestural IPH views.
  GestureInProductHelpView* _pullToRefreshGestureIPH;
  GestureInProductHelpView* _swipeBackForwardGestureIPH;
  ToolbarSwipeGestureInProductHelpView* _toolbarSwipeGestureIPH;
}

- (instancetype)
        initWithLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                engagementTracker:
                    (raw_ptr<feature_engagement::Tracker>)engagementTracker
                     webStateList:(raw_ptr<WebStateList>)webStateList
    overlayPresenterForWebContent:
        (raw_ptr<OverlayPresenter>)webContentOverlayPresenter
                    infobarBanner:(raw_ptr<OverlayPresenter>)bannerPresenter
                     infobarModal:(raw_ptr<OverlayPresenter>)modalPresenter {
  self = [super init];
  if (self) {
    CHECK(webStateList);

    _layoutGuideCenter = layoutGuideCenter;
    _engagementTracker = engagementTracker;
    _webStateList = webStateList;

    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);

    // Set and observe overlay presenters.
    if (webContentOverlayPresenter) {
      CHECK(webContentOverlayPresenter->GetModality() ==
            OverlayModality::kWebContentArea);
      _webContentOverlayPresenter = webContentOverlayPresenter;
      _webContentOverlayPresenter->AddObserver(_overlayPresenterObserver.get());
    }
    if (bannerPresenter) {
      CHECK(bannerPresenter->GetModality() == OverlayModality::kInfobarBanner);
      _infobarBannerPresenter = bannerPresenter;
      _infobarBannerPresenter->AddObserver(_overlayPresenterObserver.get());
    }
    if (modalPresenter) {
      CHECK(modalPresenter->GetModality() == OverlayModality::kInfobarModal);
      _infobarModalPresenter = modalPresenter;
      _infobarModalPresenter->AddObserver(_overlayPresenterObserver.get());
    }

    _started = YES;
  }
  return self;
}

- (void)disconnect {
  _started = NO;
  [self disconnectOverlayPresenters];
  _webStateList = nullptr;
  _engagementTracker = nullptr;
}

- (void)hideAllHelpBubbles {
  [_bottomToolbarTipBubblePresenter dismissAnimated:NO];
  [_discoverFeedHeaderMenuTipBubblePresenter dismissAnimated:NO];
  [_homeCustomizationMenuTipBubblePresenter dismissAnimated:NO];
  [_readingListTipBubblePresenter dismissAnimated:NO];
  [_followWhileBrowsingBubbleTipPresenter dismissAnimated:NO];
  [_priceNotificationsWhileBrowsingBubbleTipPresenter dismissAnimated:NO];
  [_whatsNewBubblePresenter dismissAnimated:NO];
  [_lensKeyboardPresenter dismissAnimated:NO];
  [_defaultPageModeTipBubblePresenter dismissAnimated:NO];
  [_parcelTrackingTipBubblePresenter dismissAnimated:NO];
  [_lensOverlayEntrypointBubblePresenter dismissAnimated:NO];
  [self hideAllGestureInProductHelpViewsForReason:IPHDismissalReasonType::
                                                      kUnknown];
}

- (void)handleTapOutsideOfVisibleGestureInProductHelp {
  [self hideAllGestureInProductHelpViewsForReason:
            IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView];
}

- (void)handleToolbarSwipeGesture {
  [_toolbarSwipeGestureIPH
      dismissWithReason:IPHDismissalReasonType::
                            kSwipedAsInstructedByGestureIPH];
}

#pragma mark - Bubble presenter methods

- (void)presentDiscoverFeedMenuTipBubble {
  BubbleArrowDirection arrowDirection = IsHomeCustomizationEnabled()
                                            ? BubbleArrowDirectionUp
                                            : BubbleArrowDirectionDown;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_DISCOVER_FEED_HEADER_IPH);

  UIView* menuButton =
      [_layoutGuideCenter referencedViewUnderName:kFeedIPHNamedGuide];
  // Checks "canPresentBubble" after checking that the NTP with feed is visible.
  // This ensures that the feature tracker doesn't trigger the IPH event if the
  // bubble isn't shown, which would prevent it from ever being shown again.
  if (!menuButton || ![self canPresentBubble]) {
    return;
  }
  CGPoint discoverFeedMenuAnchor =
      [menuButton.superview convertPoint:menuButton.frame.origin toView:nil];

  // Slightly move IPH to ensure that the bubble doesn't bleed out the screen.
  if (IsHomeCustomizationEnabled()) {
    discoverFeedMenuAnchor.x += menuButton.frame.size.width / 2;
    discoverFeedMenuAnchor.y += menuButton.frame.size.height;
  } else {
    discoverFeedMenuAnchor.x += menuButton.frame.size.width / 3;
  }

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `discoverFeedHeaderMenuTipBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHDiscoverFeedHeaderFeature
                    direction:arrowDirection
                    alignment:IsHomeCustomizationEnabled()
                                  ? BubbleAlignmentTopOrLeading
                                  : BubbleAlignmentBottomOrTrailing
                         text:text
        voiceOverAnnouncement:text
                  anchorPoint:discoverFeedMenuAnchor
                presentAction:nil
                dismissAction:nil];
  if (!presenter)
    return;

  _discoverFeedHeaderMenuTipBubblePresenter = presenter;
}

- (void)presentHomeCustomizationTipBubble {
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_HOME_CUSTOMIZATION_IPH);

  UIView* menuButton =
      [_layoutGuideCenter referencedViewUnderName:kFeedIPHNamedGuide];
  // Checks "canPresentBubble" after checking that the NTP with feed is visible.
  // This ensures that the feature tracker doesn't trigger the IPH event if the
  // bubble isn't shown, which would prevent it from ever being shown again.
  if (!menuButton || ![self canPresentBubble]) {
    return;
  }
  CGPoint customizationMenuAnchor =
      [menuButton.superview convertPoint:menuButton.frame.origin toView:nil];

  // Slightly move IPH to ensure that the bubble doesn't bleed out the screen.
  customizationMenuAnchor.x += menuButton.frame.size.width / 2;
  customizationMenuAnchor.y += menuButton.frame.size.height;

  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:feature_engagement::
                                        kIPHHomeCustomizationMenuFeature
                          direction:BubbleArrowDirectionUp
                          alignment:BubbleAlignmentTopOrLeading
                               text:text
              voiceOverAnnouncement:text
                        anchorPoint:customizationMenuAnchor
                      presentAction:nil
                      dismissAction:nil];
  if (!presenter) {
    return;
  }

  _homeCustomizationMenuTipBubblePresenter = presenter;
}

- (void)presentFollowWhileBrowsingTipBubbleAndLogWithRecorder:
            (FeedMetricsRecorder*)recorder
                                             popupMenuHandler:
                                                 (id<PopupMenuCommands>)
                                                     popupMenuHandler {
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
  if (presenter) {
    [popupMenuHandler notifyIPHBubblePresenting];
    _followWhileBrowsingBubbleTipPresenter = presenter;
  }
  [recorder recordFollowRecommendationIPHShown];
}

- (void)presentDefaultSiteViewTipBubbleWithSettingsMap:
            (raw_ptr<HostContentSettingsMap>)settingsMap
                                      popupMenuHandler:(id<PopupMenuCommands>)
                                                           popupMenuHandler {
  if (![self canPresentBubble]) {
    return;
  }
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (!currentWebState || ShouldLoadUrlInDesktopMode(
                              currentWebState->GetVisibleURL(), settingsMap)) {
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
  [popupMenuHandler notifyIPHBubblePresenting];
  _defaultPageModeTipBubblePresenter = presenter;
}

- (void)presentWhatsNewBottomToolbarBubbleWithPopupMenuHandler:
    (id<PopupMenuCommands>)popupMenuHandler {
  if (![self canPresentBubble]) {
    return;
  }
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
  if (presenter) {
    [popupMenuHandler notifyIPHBubblePresenting];
    _whatsNewBubblePresenter = presenter;
  }
}

- (void)presentPriceNotificationsWhileBrowsingTipBubbleWithPopupMenuHandler:
    (id<PopupMenuCommands>)popupMenuHandler {
  if (![self canPresentBubble]) {
    return;
  }
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
  if (presenter) {
    [popupMenuHandler notifyIPHBubblePresenting];
    _priceNotificationsWhileBrowsingBubbleTipPresenter = presenter;
  }
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
  if (presenter) {
    _lensKeyboardPresenter = presenter;
  }
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
    _parcelTrackingTipBubblePresenter = presenter;
  }
}

- (void)presentLensOverlayTipBubble {
  if (![self canPresentBubble]) {
    return;
  }

  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }

  BOOL isBottomOmnibox = IsBottomOmniboxAvailable() &&
                         GetApplicationContext()->GetLocalState()->GetBoolean(
                             prefs::kBottomOmnibox);
  BubbleArrowDirection arrowDirection =
      isBottomOmnibox ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_TOOLTIP_TEXT);

  CGPoint lensOverlayEntrypointAnchor =
      [self anchorPointToGuide:kLensOverlayEntrypointGuide
                     direction:arrowDirection];
  // To prevent the bubble from extending beyond the screen's edge, an offset is
  // added, with the anchor point positioned at the top left corner.
  // TODO(crbug.com/365049480): Remove this offset once the bubble view margins
  // are fixed.
  CGFloat anchorXOffset = UseRTLLayout() ? -2 : 2;

  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::
                                  kIPHiOSLensOverlayEntrypointTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTopOrLeading
                         text:text
        voiceOverAnnouncement:text
                  anchorPoint:CGPoint(
                                  lensOverlayEntrypointAnchor.x + anchorXOffset,
                                  lensOverlayEntrypointAnchor.y)
                presentAction:nil
                dismissAction:nil];

  if (presenter) {
    _lensOverlayEntrypointBubblePresenter = presenter;
  }
}

- (void)
    presentPullToRefreshGestureInProductHelpWithDeviceSwitcherResultDispatcher:
        (raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>)
            deviceSwitcherResultDispatcher {
  if (UIAccessibilityIsVoiceOverRunning() ||
      (![self.delegate isOverscrollActionsSupportedForBubblePresenter:self]) ||
      (![self canPresentBubble])) {
    // TODO(crbug.com/41494458): Add voice over announcement once fixed.
    return;
  }
  const base::Feature& pullToRefreshFeature =
      feature_engagement::kIPHiOSPullToRefreshFeature;
  BOOL userEligibleForPullToRefreshIPH =
      deviceSwitcherResultDispatcher &&
      iph_for_new_chrome_user::IsUserNewSafariSwitcher(
          deviceSwitcherResultDispatcher) &&
      _engagementTracker->WouldTriggerHelpUI(pullToRefreshFeature);
  if (!userEligibleForPullToRefreshIPH) {
    return;
  }
  NSString* text = l10n_util::GetNSString(IDS_IOS_PULL_TO_REFRESH_IPH);
  _pullToRefreshGestureIPH =
      [self presentGestureInProductHelpForFeature:pullToRefreshFeature
                                   swipeDirection:
                                       UISwipeGestureRecognizerDirectionDown
                                             text:text];
  [_pullToRefreshGestureIPH startAnimation];
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
      _engagementTracker->WouldTriggerHelpUI(backForwardSwipeFeature);
  if (!userEligible) {
    return;
  }

  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }

  // Retrieve swipe-able directions.
  const web::NavigationManager* navigationManager =
      currentWebState->GetNavigationManager();
  BOOL back = navigationManager->CanGoBack();
  BOOL forward = navigationManager->CanGoForward();
  if (!back && !forward) {
    return;
  }
  int textId = IDS_IOS_BACK_FORWARD_SWIPE_IPH_BACK_ONLY;
  if (forward) {
    textId = back ? IDS_IOS_BACK_FORWARD_SWIPE_IPH
                  : IDS_IOS_BACK_FORWARD_SWIPE_IPH_FORWARD_ONLY;
  }

  UISwipeGestureRecognizerDirection direction =
      back ^ UseRTLLayout() ? UISwipeGestureRecognizerDirectionRight
                            : UISwipeGestureRecognizerDirectionLeft;
  _swipeBackForwardGestureIPH = [self
      presentGestureInProductHelpForFeature:backForwardSwipeFeature
                             swipeDirection:direction
                                       text:l10n_util::GetNSString(textId)];
  _swipeBackForwardGestureIPH.edgeSwipe = YES;
  if (back && forward) {
    _swipeBackForwardGestureIPH.animationRepeatCount = 4;
    _swipeBackForwardGestureIPH.bidirectional = YES;
  }
  [_swipeBackForwardGestureIPH startAnimation];
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
                      _engagementTracker->WouldTriggerHelpUI(feature);
  if (!userEligible) {
    return;
  }
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (IsUrlNtp(currentWebState->GetVisibleURL())) {
    return;
  }

  // Check index to determine which directions are supported.
  int activeIndex = _webStateList->active_index();
  BOOL canGoBack = activeIndex > 0;
  BOOL canGoForward = activeIndex < _webStateList->count() - 1;
  if (!canGoBack && !canGoForward) {
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

  // Configure IPH view.
  ToolbarSwipeGestureInProductHelpView* toolbarSwipeGestureIPH =
      [[ToolbarSwipeGestureInProductHelpView alloc]
          initWithBubbleBoundingSize:guide.layoutFrame.size
                           canGoBack:canGoBack
                             forward:canGoForward];
  [toolbarSwipeGestureIPH setTranslatesAutoresizingMaskIntoConstraints:NO];
  if (!CanGestureInProductHelpViewFitInGuide(toolbarSwipeGestureIPH, guide) ||
      !_engagementTracker->ShouldTriggerHelpUI(feature)) {
    return;
  }
  toolbarSwipeGestureIPH.topConstraintForBottomEdgeSwipe =
      topConstraintForBottomEdgeSwipe;
  toolbarSwipeGestureIPH.topConstraintForTopEdgeSwipe =
      topConstraintForTopEdgeSwipe;
  toolbarSwipeGestureIPH.delegate = self;
  [self.rootViewController.view addSubview:toolbarSwipeGestureIPH];
  AddSameConstraints(toolbarSwipeGestureIPH, guide);

  [toolbarSwipeGestureIPH startAnimation];
  _toolbarSwipeGestureIPH = toolbarSwipeGestureIPH;
}

#pragma mark - GestureInProductHelpViewDelegate

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
            didDismissWithReason:(IPHDismissalReasonType)reason {
  const feature_engagement::Tracker::SnoozeAction snoozeAction =
      feature_engagement::Tracker::SnoozeAction::DISMISSED;
  std::string dismissButtonTappedEvent;
  if (view == _pullToRefreshGestureIPH) {
    dismissButtonTappedEvent =
        feature_engagement::events::kIOSPullToRefreshIPHDismissButtonTapped;
    [self featureDismissed:feature_engagement::kIPHiOSPullToRefreshFeature
                withSnooze:snoozeAction];
  } else if (view == _swipeBackForwardGestureIPH) {
    dismissButtonTappedEvent =
        feature_engagement::events::kIOSSwipeBackForwardIPHDismissButtonTapped;
    [self featureDismissed:feature_engagement::kIPHiOSSwipeBackForwardFeature
                withSnooze:snoozeAction];
  } else if (view == _toolbarSwipeGestureIPH) {
    dismissButtonTappedEvent = feature_engagement::events::
        kIOSSwipeToolbarToChangeTabIPHDismissButtonTapped;
    [self featureDismissed:feature_engagement::
                               kIPHiOSSwipeToolbarToChangeTabFeature
                withSnooze:snoozeAction];
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  if (reason == IPHDismissalReasonType::kTappedClose && _engagementTracker &&
      !dismissButtonTappedEvent.empty()) {
    _engagementTracker->NotifyEvent(dismissButtonTappedEvent);
  }
}

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
    shouldHandleSwipeInDirection:(UISwipeGestureRecognizerDirection)direction {
  if (view == _pullToRefreshGestureIPH) {
    [self.delegate bubblePresenterDidPerformPullToRefreshGesture:self];
  } else if (view == _swipeBackForwardGestureIPH) {
    [self.delegate bubblePresenter:self
        didPerformSwipeToNavigateInDirection:direction];
  } else if (view == _toolbarSwipeGestureIPH) {
    // Do nothing. Swipe happens outside of the view.
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  [self hideAllHelpBubbles];
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  switch (presenter->GetModality()) {
    case OverlayModality::kWebContentArea:
      CHECK_EQ(presenter, _webContentOverlayPresenter);
      _webContentOverlayPresenter = nullptr;
      break;
    case OverlayModality::kInfobarBanner:
      CHECK_EQ(presenter, _infobarBannerPresenter);
      _infobarBannerPresenter = nullptr;
      break;
    case OverlayModality::kInfobarModal:
      CHECK_EQ(presenter, _infobarModalPresenter);
      _infobarModalPresenter = nullptr;
      break;
    case OverlayModality::kTesting:
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
  DCHECK(_engagementTracker);
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
       _engagementTracker->ShouldTriggerHelpUI(feature))) {
    [presenter presentInViewController:self.rootViewController
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
  [_pullToRefreshGestureIPH dismissWithReason:reason];
  [_swipeBackForwardGestureIPH dismissWithReason:reason];
  [_toolbarSwipeGestureIPH dismissWithReason:reason];
}

#pragma mark - Private Utils

// Returns the anchor point for a bubble with an `arrowDirection` pointing to a
// `guideName`. The point is in the window coordinates.
- (CGPoint)anchorPointToGuide:(GuideName*)guideName
                    direction:(BubbleArrowDirection)arrowDirection {
  UILayoutGuide* guide = [_layoutGuideCenter makeLayoutGuideNamed:guideName];
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
// TODO(crbug.com/40914423): make most callsites pass NO for
// `CheckTabScrolledToTop` as it's error-prone.
- (BOOL)canPresentBubble {
  return [self canPresentBubbleWithCheckTabScrolledToTop:YES];
}

// Returns whether the tab can present a bubble tip. Whether tab being scrolled
// to top is required for presenting the bubble tip is determined by
// `checkTabScrolledToTop`.
- (BOOL)canPresentBubbleWithCheckTabScrolledToTop:(BOOL)checkTabScrolledToTop {
  // If BubblePresenter has been stopped, do not present the bubble.
  if (!_started) {
    return NO;
  }
  // If the BVC is not visible, do not present the bubble.
  if (![self.delegate rootViewVisibleForBubblePresenter:self]) {
    return NO;
  }
  // Do not present the bubble if there is no current tab.
  if (!_webStateList->GetActiveWebState()) {
    return NO;
  }
  // Do not present bubble if an overlay is showing.
  if ((_webContentOverlayPresenter &&
       _webContentOverlayPresenter->IsShowingOverlayUI()) ||
      (_infobarBannerPresenter &&
       _infobarBannerPresenter->IsShowingOverlayUI()) ||
      (_infobarModalPresenter &&
       _infobarModalPresenter->IsShowingOverlayUI())) {
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
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
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
  DCHECK(_engagementTracker);
  // Capture `weakSelf` instead of the feature engagement tracker object
  // because `weakSelf` will safely become `nil` if it is deallocated, whereas
  // the feature engagement tracker will remain pointing to invalid memory if
  // its owner (the ProfileIOS) is deallocated.
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
                  dismissalCallback:dismissalCallbackWithSnoozeAction];

  bubbleViewControllerPresenter.customBubbleVisibilityDuration =
      [self bubbleVisibilityDurationForFeature:feature];

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
  DCHECK(_engagementTracker);
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
      _engagementTracker->ShouldTriggerHelpUI(feature)) {
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
  if (!_engagementTracker) {
    return;
  }
  _engagementTracker->DismissedWithSnooze(feature, snoozeAction);
}

// Returns the custom duration of the bubble for `feature`, or 0 if there is
// none.
- (NSTimeInterval)bubbleVisibilityDurationForFeature:
    (const base::Feature&)feature {
  // Display FollowWhileBrowsing in-product help bubble with custom duration.
  if (feature.name == feature_engagement::kIPHFollowWhileBrowsingFeature.name) {
    return kDefaultLongDurationBubbleVisibility;
  }

  return 0;
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

// Stop observing overlay events and disconnect related properties.
- (void)disconnectOverlayPresenters {
  if (_webContentOverlayPresenter) {
    _webContentOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    _webContentOverlayPresenter = nullptr;
  }
  if (_infobarBannerPresenter) {
    _infobarBannerPresenter->RemoveObserver(_overlayPresenterObserver.get());
    _infobarBannerPresenter = nullptr;
  }
  if (_infobarModalPresenter) {
    _infobarModalPresenter->RemoveObserver(_overlayPresenterObserver.get());
    _infobarModalPresenter = nullptr;
  }
  _overlayPresenterObserver = nullptr;
}

@end
