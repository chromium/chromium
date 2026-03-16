// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_container_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"

@implementation AppBarContainerMediator {
  raw_ptr<FullscreenController> _regularFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _regularFullscreenUIUpdater;
  raw_ptr<FullscreenController> _incognitoFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _incognitoFullscreenUIUpdater;
}

- (instancetype)initWithRegularFullscreenController:
                    (FullscreenController*)regularFullscreenController
                      incognitoFullscreenController:
                          (FullscreenController*)incognitoFullscreenController {
  self = [super init];
  if (self) {
    _regularFullscreenController = regularFullscreenController;
    _incognitoFullscreenController = incognitoFullscreenController;
  }
  return self;
}

- (void)setConsumer:(id<FullscreenUIElement>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  _regularFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      _regularFullscreenController, _consumer);
  if (_incognitoFullscreenController) {
    _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _incognitoFullscreenController, _consumer);
  }
}

- (void)disconnect {
  self.consumer = nil;
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenController = nullptr;
  _incognitoFullscreenController = nullptr;
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

@end
