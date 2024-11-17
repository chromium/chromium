// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_coordinator.h"

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ui/base/device_form_factor.h"

@interface BubblePresenterCoordinator () <HelpCommands>

@end

@implementation BubblePresenterCoordinator {
  BubblePresenter* _presenter;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  OverlayPresenter* webContentPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  OverlayPresenter* infobarBannerPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kInfobarBanner);
  OverlayPresenter* infobarModalPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kInfobarModal);

  _presenter = [[BubblePresenter alloc]
          initWithLayoutGuideCenter:LayoutGuideCenterForBrowser(self.browser)
                  engagementTracker:engagementTracker
                       webStateList:self.browser->GetWebStateList()
      overlayPresenterForWebContent:webContentPresenter
                      infobarBanner:infobarBannerPresenter
                       infobarModal:infobarModalPresenter];

  _presenter.delegate = self.bubblePresenterDelegate;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(HelpCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(HelpCommands)];
  [_presenter hideAllHelpBubbles];
  [_presenter disconnect];
  _presenter = nil;
}

#pragma mark - Accessors

- (UIViewController*)baseViewController {
  return _presenter.rootViewController;
}

- (void)setBaseViewController:(UIViewController*)baseViewController {
  CHECK(_presenter);
  _presenter.rootViewController = baseViewController;
}

#pragma mark - HelpCommands

- (void)presentInProductHelpWithType:(InProductHelpType)type {
  ProfileIOS* profile = self.browser->GetProfile();
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      deviceSwitcherResultDispatcher = nullptr;
  if (!profile->IsOffTheRecord()) {
    deviceSwitcherResultDispatcher = segmentation_platform::
        SegmentationPlatformServiceFactory::GetDispatcherForProfile(profile);
  }
  CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
  id<PopupMenuCommands> popupMenuHandler =
      HandlerForProtocol(commandDispatcher, PopupMenuCommands);
  switch (type) {
    case InProductHelpType::kDiscoverFeedMenu: {
      [_presenter presentDiscoverFeedMenuTipBubble];
      break;
    }
    case InProductHelpType::kHomeCustomizationMenu: {
      [_presenter presentHomeCustomizationTipBubble];
      break;
    }
    case InProductHelpType::kFollowWhileBrowsing: {
      [_presenter presentFollowWhileBrowsingTipBubbleAndLogWithRecorder:
                      DiscoverFeedServiceFactory::GetForProfile(profile)
                          ->GetFeedMetricsRecorder()
                                                       popupMenuHandler:
                                                           popupMenuHandler];
      break;
    }
    case InProductHelpType::kDefaultSiteView: {
      [_presenter
          presentDefaultSiteViewTipBubbleWithSettingsMap:
              ios::HostContentSettingsMapFactory::GetForProfile(profile)
                                        popupMenuHandler:popupMenuHandler];
      break;
    }
    case InProductHelpType::kWhatsNew: {
      [_presenter presentWhatsNewBottomToolbarBubbleWithPopupMenuHandler:
                      popupMenuHandler];
      break;
    }
    case InProductHelpType::kPriceNotificationsWhileBrowsing: {
      [_presenter
          presentPriceNotificationsWhileBrowsingTipBubbleWithPopupMenuHandler:
              popupMenuHandler];
      break;
    }
    case InProductHelpType::kLensKeyboard: {
      [_presenter presentLensKeyboardTipBubble];
      break;
    }
    case InProductHelpType::kParcelTracking: {
      [_presenter presentParcelTrackingTipBubble];
      break;
    }
    case InProductHelpType::kPullToRefresh: {
      [_presenter
          presentPullToRefreshGestureInProductHelpWithDeviceSwitcherResultDispatcher:
              deviceSwitcherResultDispatcher];
      break;
    }
    case InProductHelpType::kBackForwardSwipe: {
      [_presenter presentBackForwardSwipeGestureInProductHelp];
      break;
    }
    case InProductHelpType::kToolbarSwipe: {
      [_presenter presentToolbarSwipeGestureInProductHelp];
      break;
    }
    case InProductHelpType::kLensOverlayEntrypoint: {
      [_presenter presentLensOverlayTipBubble];
      break;
    }
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
