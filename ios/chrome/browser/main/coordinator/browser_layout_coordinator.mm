// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/coordinator/browser_layout_coordinator.h"

#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"
#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_utils.h"
#import "ui/base/device_form_factor.h"

@implementation BrowserLayoutCoordinator {
  // Coordinator for the infobar banner overlay container.
  OverlayContainerCoordinator* _infobarBannerOverlayContainerCoordinator;
  // Coordinator for the infobar modal overlay container.
  OverlayContainerCoordinator* _infobarModalOverlayContainerCoordinator;
  // Coordinator for the tab strip.
  TabStripCoordinator* _tabStripCoordinator;
  // Observer for the fullscreen controller.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
  // Bridge to observe the FullscreenBrowserAgent.
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _fullscreenBrowserAgentObserverBridge;
  SafeAreaProvider* _safeAreaProvider;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  Browser* browser = self.browser;

  _safeAreaProvider = [[SafeAreaProvider alloc] initWithBrowser:browser];

  BrowserLayoutViewController* viewController =
      [[BrowserLayoutViewController alloc] init];
  viewController.incognito = browser->GetProfile()->IsOffTheRecord();
  viewController.safeAreaProvider = _safeAreaProvider;
  _viewController = viewController;

  SceneState* sceneState = browser->GetSceneState();
  viewController.layoutState = sceneState.layoutState;

  if (IsFullscreenRefactoringEnabled()) {
    FullscreenBrowserAgent* agent =
        FullscreenBrowserAgent::FromBrowser(browser);
    _fullscreenBrowserAgentObserverBridge =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(viewController,
                                                               agent);
  }

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);
  if (fullscreenController) {
    _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        fullscreenController, viewController);
  }

  _infobarModalOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:viewController
                             browser:browser
                            modality:OverlayModality::kInfobarModal];
  [_infobarModalOverlayContainerCoordinator start];
  viewController.infobarModalOverlayContainerViewController =
      _infobarModalOverlayContainerCoordinator.viewController;

  _infobarBannerOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:viewController
                             browser:browser
                            modality:OverlayModality::kInfobarBanner];
  [_infobarBannerOverlayContainerCoordinator start];
  viewController.infobarBannerOverlayContainerViewController =
      _infobarBannerOverlayContainerCoordinator.viewController;

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    _tabStripCoordinator =
        [[TabStripCoordinator alloc] initWithBrowser:browser];
    _tabStripCoordinator.baseViewController = viewController;
    [_tabStripCoordinator start];

    viewController.tabStripViewController = _tabStripCoordinator.viewController;
  }
}

- (void)stop {
  [_infobarModalOverlayContainerCoordinator stop];
  _infobarModalOverlayContainerCoordinator = nil;

  [_infobarBannerOverlayContainerCoordinator stop];
  _infobarBannerOverlayContainerCoordinator = nil;

  [_tabStripCoordinator stop];
  _tabStripCoordinator = nil;

  _fullscreenBrowserAgentObserverBridge = nullptr;
  _fullscreenUIUpdater = nullptr;
  _viewController = nil;
  _safeAreaProvider = nil;
}

#pragma mark - Properties

- (void)setBrowserViewController:
    (UIViewController<BrowserLayoutConsumer>*)browserViewController {
  _viewController.browserViewController = browserViewController;
}

- (UIViewController<BrowserLayoutConsumer>*)browserViewController {
  return _viewController.browserViewController;
}

@end
