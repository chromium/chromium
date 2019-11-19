// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Observer that disables fullscreen while overlays are presented over the
// kWebContentArea modality.
class WebContentAreaOverlayFullscreenDisabler
    : public OverlayPresenterObserver {
 public:
  explicit WebContentAreaOverlayFullscreenDisabler(
      FullscreenController* fullscreen_controller)
      : controller_(fullscreen_controller) {
    DCHECK(controller_);
  }

  void WillShowOverlay(OverlayPresenter* presenter,
                       OverlayRequest* request) override {
    disabler_ = std::make_unique<AnimatedScopedFullscreenDisabler>(controller_);
    disabler_->StartAnimation();
  }

  void DidHideOverlay(OverlayPresenter* presenter,
                      OverlayRequest* request) override {
    disabler_ = nullptr;
  }

 private:
  FullscreenController* controller_;
  std::unique_ptr<AnimatedScopedFullscreenDisabler> disabler_;
};
}  // namespace

@interface BrowserContainerCoordinator () {
  // The helper that disables fullscreen when overlays are presented over the
  // web content area.
  std::unique_ptr<WebContentAreaOverlayFullscreenDisabler>
      _overlayFullscreenDisabler;
}
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The overlay container coordinator for OverlayModality::kWebContentArea.
@property(nonatomic, strong)
    OverlayContainerCoordinator* webContentAreaOverlayContainerCoordinator;
@end

@implementation BrowserContainerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  DCHECK(self.browserState);
  DCHECK(!_viewController);
  BrowserContainerViewController* viewController =
      [[BrowserContainerViewController alloc] init];
  self.webContentAreaOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:viewController
                             browser:self.browser
                            modality:OverlayModality::kWebContentArea];
  [self.webContentAreaOverlayContainerCoordinator start];
  viewController.webContentsOverlayContainerViewController =
      self.webContentAreaOverlayContainerCoordinator.viewController;
  _viewController = viewController;

  _overlayFullscreenDisabler =
      std::make_unique<WebContentAreaOverlayFullscreenDisabler>(
          FullscreenControllerFactory::GetForBrowserState(
              self.browser->GetBrowserState()));
  OverlayPresenter::FromBrowser(self.browser, OverlayModality::kWebContentArea)
      ->AddObserver(_overlayFullscreenDisabler.get());

  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  [self.webContentAreaOverlayContainerCoordinator stop];
  _viewController = nil;
  OverlayPresenter::FromBrowser(self.browser, OverlayModality::kWebContentArea)
      ->RemoveObserver(_overlayFullscreenDisabler.get());
  _overlayFullscreenDisabler = nullptr;
  [super stop];
}

@end
