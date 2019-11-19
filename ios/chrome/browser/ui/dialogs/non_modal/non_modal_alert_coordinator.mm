// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_coordinator.h"

#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_presentation_updater.h"
#import "ios/chrome/browser/ui/fullscreen/chrome_coordinator+fullscreen_disabling.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NonModalAlertCoordinator () {
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}
// The non-modal presentation updater.
@property(nonatomic, strong)
    NonModalAlertPresentationUpdater* nonModalPresentationUpdater;
@end

@implementation NonModalAlertCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message
                              browserState:
                                  (ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithBaseViewController:viewController
                                     title:title
                                   message:message
                              browserState:browserState];
  return self;
}

#pragma mark - Accessors

- (void)setNonModalPresentationUpdater:
    (NonModalAlertPresentationUpdater*)nonModalPresentationUpdater {
  if (_nonModalPresentationUpdater == nonModalPresentationUpdater)
    return;
  FullscreenController* fullscreenController =
      FullscreenControllerFactory::GetInstance()->GetForBrowserState(
          self.browserState);
  _fullscreenUIUpdater = nullptr;

  _nonModalPresentationUpdater = nonModalPresentationUpdater;

  if (_nonModalPresentationUpdater) {
    // Create an updater for the new non-modal presentation controller.
    _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        fullscreenController, _nonModalPresentationUpdater);
    // Use the current viewport insets to set up the non-modal presentation.
    [_nonModalPresentationUpdater
        setUpNonModalPresentationWithViewportInsets:
            fullscreenController->GetCurrentViewportInsets()];
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  // Disable fullscreen while non-modal alerts are displayed to ensure that the
  // toolbars are fully visible and interactable.
  [self didStartFullscreenDisablingUI];

  // Create the non-modal presentation controller for the alert.
  self.nonModalPresentationUpdater = [[NonModalAlertPresentationUpdater alloc]
      initWithAlertController:self.alertController];
}

- (void)stop {
  [super stop];
  self.nonModalPresentationUpdater = nil;
  [self didStopFullscreenDisablingUI];
}

@end
