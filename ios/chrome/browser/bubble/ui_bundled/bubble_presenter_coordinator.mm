// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_coordinator.h"

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ui/base/device_form_factor.h"

@interface BubblePresenterCoordinator () <HelpCommands>

@end

@implementation BubblePresenterCoordinator {
  BubblePresenter* _presenter;
}

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  // TODO(crbug.com/40272358): refactor by instantiating bubble dependencies as
  // needed.
  segmentation_platform::DeviceSwitcherResultDispatcher*
      deviceSwitcherResultDispatcher = nullptr;
  if (!browserState->IsOffTheRecord()) {
    deviceSwitcherResultDispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForBrowserState(browserState);
  }
  id<TabStripCommands> tabStripCommandsHandler = nil;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // The -startDispatching for TabStripCommands will be called at a later
    // point for tablet only, so cannot use `HandlerForProtocol`.
    tabStripCommandsHandler =
        static_cast<id<TabStripCommands>>(self.browser->GetCommandDispatcher());
  }
  HostContentSettingsMap* settingsMap =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);

  _presenter = [[BubblePresenter alloc]
      initWithDeviceSwitcherResultDispatcher:deviceSwitcherResultDispatcher
                      hostContentSettingsMap:settingsMap
                     tabStripCommandsHandler:tabStripCommandsHandler
                                     tracker:engagementTracker
                                webStateList:self.browser->GetWebStateList()];
  _presenter.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _presenter.webContentOverlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  _presenter.infobarBannerPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kInfobarBanner);
  _presenter.infobarModalPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kInfobarModal);
  _presenter.delegate = self.bubblePresenterDelegate;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(HelpCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(HelpCommands)];
  [_presenter stop];
}

- (void)setBaseViewController:(UIViewController*)baseViewController {
  _baseViewController = baseViewController;
  _presenter.rootViewController = baseViewController;
}

#pragma mark - HelpCommands

- (void)presentInProductHelpWithType:(InProductHelpType)type {
  switch (type) {
    case InProductHelpType::kDiscoverFeedMenu:
      [_presenter presentDiscoverFeedMenuTipBubble];
      break;
    case InProductHelpType::kFollowWhileBrowsing:
      [_presenter presentFollowWhileBrowsingTipBubble];
      break;
    case InProductHelpType::kDefaultSiteView:
      [_presenter presentDefaultSiteViewTipBubble];
      break;
    case InProductHelpType::kWhatsNew:
      [_presenter presentWhatsNewBottomToolbarBubble];
      break;
    case InProductHelpType::kPriceNotificationsWhileBrowsing:
      [_presenter presentPriceNotificationsWhileBrowsingTipBubble];
      break;
    case InProductHelpType::kLensKeyboard:
      [_presenter presentLensKeyboardTipBubble];
      break;
    case InProductHelpType::kParcelTracking:
      [_presenter presentParcelTrackingTipBubble];
      break;
    case InProductHelpType::kShareButton:
      [_presenter presentShareButtonHelpBubbleIfEligible];
      break;
    case InProductHelpType::kTabGridToolbarItem:
      [_presenter presentTabGridToolbarItemBubble];
      break;
    case InProductHelpType::kNewTabToolbarItem:
      [_presenter presentNewTabToolbarItemBubble];
      break;
    case InProductHelpType::kPullToRefresh:
      [_presenter presentPullToRefreshGestureInProductHelp];
      break;
    case InProductHelpType::kBackForwardSwipe:
      [_presenter presentBackForwardSwipeGestureInProductHelp];
      break;
    case InProductHelpType::kToolbarSwipe:
      [_presenter presentToolbarSwipeGestureInProductHelp];
      break;
  }
}

- (void)hideAllHelpBubbles {
  [_presenter hideAllHelpBubbles];
}

- (void)handleTapOutsideOfVisibleGestureInProductHelp {
  [_presenter handleTapOutsideOfVisibleGestureInProductHelp];
}

- (void)handleToolbarSwipeGesture {
  [_presenter handleToolbarSwipeGesture];
}

@end
