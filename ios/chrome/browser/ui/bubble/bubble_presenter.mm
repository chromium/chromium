// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"

#import "base/functional/bind.h"
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
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/bubble/side_swipe_bubble/side_swipe_bubble_view.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer_bridge.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns whether `view` could display and animate correctly within `guide`. If
// NO, elements in `view` may be hidden or overlap with each other during the
// animation.
BOOL CanSideSwipeBubbleViewFitInGuide(SideSwipeBubbleView* view,
                                      UILayoutGuide* guide) {
  CGSize guide_size = guide.layoutFrame.size;
  CGSize view_fitting_size =
      [view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  return view_fitting_size.width <= guide_size.width &&
         view_fitting_size.height <= guide_size.height;
}

}  // namespace

@interface BubblePresenter () <CRWWebStateObserver,
                               SceneStateObserver,
                               URLLoadingObserver>

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
@property(nonatomic, strong) SideSwipeBubbleView* pullToRefreshSideSwipeBubble;
@property(nonatomic, assign) WebStateList* webStateList;
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;
@property(nonatomic, assign) HostContentSettingsMap* settingsMap;
// Whether the presenter is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

@end

@implementation BubblePresenter {
  std::unique_ptr<UrlLoadingObserverBridge> _loadingObserverBridge;
  UrlLoadingNotifierBrowserAgent* _loadingNotifier;

  segmentation_platform::DeviceSwitcherResultDispatcher*
      _deviceSwitcherResultDispatcher;

  PrefService* _prefService;

  id<TabStripCommands> _tabStripCommandsHandler;

  web::WebState* _pullToRefreshIPHWebState;
  std::unique_ptr<web::WebStateObserverBridge>
      _pullToRefreshIPHWebStateObserver;
}

#pragma mark - Public

- (instancetype)
    initWithDeviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)
            deviceSwitcherResultDispatcher
                    hostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                           loadingNotifier:(UrlLoadingNotifierBrowserAgent*)
                                               urlLoadingNotifier
                               prefService:(PrefService*)prefService
                                sceneState:(SceneState*)sceneState
                   tabStripCommandsHandler:
                       (id<TabStripCommands>)tabStripCommandsHandler
                                   tracker:(feature_engagement::Tracker*)
                                               engagementTracker
                              webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(prefService);
    DCHECK(webStateList);
    DCHECK(urlLoadingNotifier);

    _webStateList = webStateList;
    _engagementTracker = engagementTracker;
    _settingsMap = settingsMap;
    _deviceSwitcherResultDispatcher = deviceSwitcherResultDispatcher;
    _prefService = prefService;
    _tabStripCommandsHandler = tabStripCommandsHandler;
    self.started = YES;

    _loadingObserverBridge = std::make_unique<UrlLoadingObserverBridge>(self);
    _loadingNotifier = urlLoadingNotifier;
    _loadingNotifier->AddObserver(_loadingObserverBridge.get());

    [sceneState addObserver:self];

    _pullToRefreshIPHWebState = nullptr;
    _pullToRefreshIPHWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)stop {
  self.started = NO;
  self.webStateList = nullptr;
  self.engagementTracker = nullptr;
  self.settingsMap = nullptr;

  _loadingNotifier->RemoveObserver(_loadingObserverBridge.get());
  _loadingObserverBridge.reset();

  [self removeAndUntriggerPullToRefreshIPH];
}

- (void)hideAllHelpBubbles {
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
  [self.pullToRefreshSideSwipeBubble
      dismissWithReason:IPHDismissalReasonType::kUnknown];
}

- (void)presentShareButtonHelpBubbleIfEligible {
  if (!iph_for_new_chrome_user::IsUserEligible(
          _deviceSwitcherResultDispatcher)) {
    return;
  }

  UIView* shareButtonView =
      [_layoutGuideCenter referencedViewUnderName:kShareButtonGuide];
  // Do not present if the share button is not visible.
  if (!shareButtonView || shareButtonView.hidden) {
    return;
  }

  // Do not present if button is disabled.
  CHECK([shareButtonView isKindOfClass:[UIButton class]]);
  UIButton* shareButton = (UIButton*)shareButtonView;
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
  CGPoint shareButtonAnchor = [self anchorPointToGuide:kShareButtonGuide
                                             direction:arrowDirection];

  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHiOSShareToolbarItemFeature
                    direction:arrowDirection
                         text:text
        voiceOverAnnouncement:nil
                  anchorPoint:shareButtonAnchor];
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
      referencedViewUnderName:kDiscoverFeedHeaderMenuGuide];
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

- (void)notifyMultiGestureRefreshAndShowHelpBubbleIfEligible {
  self.engagementTracker->NotifyEvent(
      feature_engagement::events::kIOSMultiGestureRefreshUsed);
  BOOL userEligibleForPullToRefreshIPH =
      iph_for_new_chrome_user::IsUserEligible(
          _deviceSwitcherResultDispatcher) &&
      self.engagementTracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSPullToRefreshFeature);
  if (!userEligibleForPullToRefreshIPH) {
    return;
  }
  _pullToRefreshIPHWebState = self.webStateList->GetActiveWebState();
  if (_pullToRefreshIPHWebState) {
    _pullToRefreshIPHWebState->AddObserver(
        _pullToRefreshIPHWebStateObserver.get());
  }
}

#pragma mark - Private

// Remove pull-to-refresh IPH if showing. Also, if
// `_pullToRefreshIPHWebStateObserver` is observing the currently active web
// state, unobserve it.
- (void)removeAndUntriggerPullToRefreshIPH {
  if (_pullToRefreshIPHWebState) {
    _pullToRefreshIPHWebState->RemoveObserver(
        _pullToRefreshIPHWebStateObserver.get());
  }
  _pullToRefreshIPHWebState = nullptr;
  _pullToRefreshIPHWebStateObserver.reset();

  if (self.pullToRefreshSideSwipeBubble) {
    // TODO(crbug.com/1467873): Add a new reason type and use that.
    [self.pullToRefreshSideSwipeBubble
        dismissWithReason:IPHDismissalReasonType::kUnknown];
  }
}

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
  if (!presenter)
    return nil;
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

// Optionally presents a bubble associated with the new tab iph. If the feature
// engagement tracker determines it is valid to show the new tab tip, then it
// initializes `openNewTabIPHBubblePresenter` and presents the bubble. If it is
// not valid to show the new tab tip, `openNewTabIPHBubblePresenter` is set to
// `nil` and no bubble is shown. This method requires that `self.browserState`
// is not NULL.
- (void)presentNewTabToolbarItemBubble {
  if (!iph_for_new_chrome_user::IsUserEligible(
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
  if (!currentWebState ||
      currentWebState->GetVisibleURL() == kChromeUINewTabURL) {
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
  if (!presenter)
    return;

  self.openNewTabIPHBubblePresenter = presenter;
}

// Optionally presents a bubble associated with the tab grid iph. If the feature
// engagement tracker determines it is valid to show the new tab tip, then it
// initializes `tabGridIPHBubblePresenter` and presents the bubble. If it is
// not valid to show the new tab tip, `tabGridIPHBubblePresenter` is set to
// `nil` and no bubble is shown. This method requires that `self.browserState`
// is not NULL.
- (void)presentTabGridToolbarItemBubble {
  if (!iph_for_new_chrome_user::IsUserEligible(
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

// Optionally presents a full screen IPH associated with the pull-to-refresh
// feature. If the feature engagement tracker determines it is valid to show the
// pull-to-refresh tip, then it initializes a SideSwipeBubbleView and presents
// it, otherwise it does nothing. This method requires that `self.browserState`
// is not NULL.
- (void)presentPullToRefreshSideSwipeBubble {
  if (UIAccessibilityIsVoiceOverRunning() || (![self canPresentBubble])) {
    return;
  }
  __weak BubblePresenter* weakSelf = self;
  NamedGuide* guide = [NamedGuide guideWithName:kContentAreaGuide
                                           view:self.rootViewController.view];
  NSString* text = l10n_util::GetNSString(IDS_IOS_PULL_TO_REFRESH_IPH);
  ProceduralBlock resetPullToRefreshSideSwipeBubble = ^{
    weakSelf.pullToRefreshSideSwipeBubble = nil;
  };
  self.pullToRefreshSideSwipeBubble =
      [self presentSideSwipeBubbleForFeature:feature_engagement::
                                                 kIPHiOSPullToRefreshFeature
                                   direction:BubbleArrowDirectionUp
                                        text:text
                               dismissAction:resetPullToRefreshSideSwipeBubble
                                     toGuide:guide];
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
  if ([self shouldForcePresentBubbleForFeature:feature] ||
      self.engagementTracker->WouldTriggerHelpUI(feature)) {
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
  return nil;
}

// Present a screen-covering side swipe bubble associated with an in-product
// help promotion if it is valid to show the promotion, and return the view.
// `feature` is the base::Feature object associated with the given promotion.
// `direction` is the direction the bubble's arrow is pointing. `text` is the
// text displayed by the bubble. `dismissAction` is the callback function
// invoked when the IPH is dismissed, and `guide` is used for the initial
// positioning of the side swipe bubble.
//
// TODO(crbug.com/1450600): Once kContentAreaGuide is moved to
// LayoutGuideCenter, replace the parameter `guide` with a string `guideName`.
- (SideSwipeBubbleView*)
    presentSideSwipeBubbleForFeature:(const base::Feature&)feature
                           direction:(BubbleArrowDirection)direction
                                text:(NSString*)text
                       dismissAction:(ProceduralBlock)dismissAction
                             toGuide:(UILayoutGuide*)guide {
  DCHECK(self.engagementTracker);
  if (!(guide && self.engagementTracker->WouldTriggerHelpUI(feature))) {
    return nil;
  }
  SideSwipeBubbleView* sideSwipeBubbleView =
      [[SideSwipeBubbleView alloc] initWithText:text
                             bubbleBoundingSize:guide.layoutFrame.size
                                 arrowDirection:direction];
  [sideSwipeBubbleView setTranslatesAutoresizingMaskIntoConstraints:NO];
  if (CanSideSwipeBubbleViewFitInGuide(sideSwipeBubbleView, guide) &&
      self.engagementTracker->ShouldTriggerHelpUI(feature)) {
    __weak BubblePresenter* weakSelf = self;
    CallbackWithIPHDismissalReasonType dismissalCallbackWithSnoozeAction =
        ^(IPHDismissalReasonType IPHDismissalReasonType,
          feature_engagement::Tracker::SnoozeAction snoozeAction) {
          if (dismissAction) {
            dismissAction();
          }
          [weakSelf featureDismissed:feature withSnooze:snoozeAction];
        };
    sideSwipeBubbleView.dismissCallback = dismissalCallbackWithSnoozeAction;
    [self.rootViewController.view addSubview:sideSwipeBubbleView];
    AddSameConstraints(sideSwipeBubbleView, guide);
    [sideSwipeBubbleView startAnimation];
    return sideSwipeBubbleView;
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

// TODO(crbug.com/1467873): Refactor observers and tab-event-based IPHs to a
// separate object.
#pragma mark - CRWWebStateObserver

- (void)webStateDidStopLoading:(web::WebState*)webState {
  CHECK_EQ(webState, _pullToRefreshIPHWebState);
  if (webState->GetLoadingProgress() == 1) {
    [self presentPullToRefreshSideSwipeBubble];
  } else {
    [self removeAndUntriggerPullToRefreshIPH];
  }
}

- (void)webStateWasHidden:(web::WebState*)webState {
  CHECK_EQ(webState, _pullToRefreshIPHWebState);
  [self removeAndUntriggerPullToRefreshIPH];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  CHECK_EQ(webState, _pullToRefreshIPHWebState);
  [self removeAndUntriggerPullToRefreshIPH];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    [self presentTabGridToolbarItemBubble];
  }
}

#pragma mark - URLLoadingObserver

- (void)tabDidLoadURL:(GURL)URL
       transitionType:(ui::PageTransition)transitionType {
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (currentWebState) {
    if ((transitionType & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
        (transitionType & ui::PAGE_TRANSITION_FORWARD_BACK)) {
      [self presentNewTabToolbarItemBubble];
    }
    GURL visible = currentWebState->GetLastCommittedURL();
    if (URL == visible &&
        transitionType & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR &&
        URL != kChromeUINewTabURL) {
      [self notifyMultiGestureRefreshAndShowHelpBubbleIfEligible];
    }
  }
}

- (void)newTabDidLoadURL:(GURL)URL isUserInitiated:(BOOL)isUserInitiated {
  if (isUserInitiated) {
    [self presentTabGridToolbarItemBubble];
  }
}

@end
