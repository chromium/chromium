// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_container_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation AppBarContainerMediator {
  raw_ptr<FullscreenController> _regularFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _regularFullscreenUIUpdater;
  raw_ptr<FullscreenController> _incognitoFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _incognitoFullscreenUIUpdater;
  raw_ptr<FullscreenBrowserAgent> _regularFullscreenBrowserAgent;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _regularFullscreenObserver;
  raw_ptr<FullscreenBrowserAgent> _incognitoFullscreenBrowserAgent;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _incognitoFullscreenObserver;
}

- (instancetype)initWithRegularFullscreenController:
                    (FullscreenController*)regularFullscreenController
                      incognitoFullscreenController:
                          (FullscreenController*)incognitoFullscreenController
                      regularFullscreenBrowserAgent:
                          (FullscreenBrowserAgent*)regularFullscreenBrowserAgent
                    incognitoFullscreenBrowserAgent:
                        (FullscreenBrowserAgent*)
                            incognitoFullscreenBrowserAgent {
  self = [super init];
  if (self) {
    _regularFullscreenController = regularFullscreenController;
    _incognitoFullscreenController = incognitoFullscreenController;
    _regularFullscreenBrowserAgent = regularFullscreenBrowserAgent;
    _incognitoFullscreenBrowserAgent = incognitoFullscreenBrowserAgent;
  }
  return self;
}

- (void)setConsumer:
    (id<FullscreenUIElement, FullscreenBrowserAgentObserving>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenObserver.reset();
  _incognitoFullscreenObserver.reset();
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  if (IsFullscreenRefactoringEnabled()) {
    _regularFullscreenObserver =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(
            _consumer, _regularFullscreenBrowserAgent);
    if (_incognitoFullscreenBrowserAgent) {
      _incognitoFullscreenObserver =
          std::make_unique<FullscreenBrowserAgentObserverBridge>(
              _consumer, _incognitoFullscreenBrowserAgent);
    }
  } else {
    _regularFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _regularFullscreenController, _consumer);
    if (_incognitoFullscreenController) {
      _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
          _incognitoFullscreenController, _consumer);
    }
  }
}

- (void)disconnect {
  self.consumer = nil;
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenObserver.reset();
  _incognitoFullscreenObserver.reset();
  _regularFullscreenController = nullptr;
  _incognitoFullscreenController = nullptr;
  _regularFullscreenBrowserAgent = nullptr;
  _incognitoFullscreenBrowserAgent = nullptr;
}

- (void)setIncognitoFullscreenController:
    (FullscreenController*)incognitoFullscreenController {
  _incognitoFullscreenUIUpdater.reset();
  _incognitoFullscreenController = incognitoFullscreenController;
  if (_incognitoFullscreenController && _consumer) {
    _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _incognitoFullscreenController, _consumer);
  }
}

- (void)setIncognitoFullscreenBrowserAgent:
    (FullscreenBrowserAgent*)fullscreenBrowserAgent {
  _incognitoFullscreenObserver.reset();
  _incognitoFullscreenBrowserAgent = fullscreenBrowserAgent;
  if (_incognitoFullscreenBrowserAgent && _consumer) {
    _incognitoFullscreenObserver =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(
            _consumer, _incognitoFullscreenBrowserAgent);
  }
}

@end
