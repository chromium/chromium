// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/coordinator/browser_layout_coordinator.h"

#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"
#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_utils.h"
#import "ui/base/device_form_factor.h"

@implementation BrowserLayoutCoordinator {
  TabStripCoordinator* _tabStripCoordinator;
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
  SafeAreaProvider* _safeAreaProvider;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  _safeAreaProvider = [[SafeAreaProvider alloc] initWithBrowser:self.browser];

  _viewController = [[BrowserLayoutViewController alloc] init];
  _viewController.incognito = self.browser->GetProfile()->IsOffTheRecord();
  _viewController.safeAreaProvider = _safeAreaProvider;

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);
  if (fullscreenController) {
    _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        fullscreenController, _viewController);
  }

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    _tabStripCoordinator =
        [[TabStripCoordinator alloc] initWithBrowser:self.browser];
    _tabStripCoordinator.baseViewController = self.viewController;
    [_tabStripCoordinator start];

    self.viewController.tabStripViewController =
        _tabStripCoordinator.viewController;
  }
}

- (void)stop {
  [_tabStripCoordinator stop];
  _tabStripCoordinator = nil;

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
