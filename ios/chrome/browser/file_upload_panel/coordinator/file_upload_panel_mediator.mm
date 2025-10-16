// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"

#import "base/check.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_observer_bridge.h"

@interface FileUploadPanelMediator () <ChooseFileControllerObserving>
@end

@implementation FileUploadPanelMediator {
  raw_ptr<ChooseFileController> _chooseFileController;
  std::unique_ptr<ChooseFileControllerObserverBridge>
      _chooseFileControllerObserverBridge;
  std::unique_ptr<base::ScopedObservation<ChooseFileController,
                                          ChooseFileController::Observer>>
      _chooseFileControllerObservation;
}

#pragma mark - Initialization

- (instancetype)initWithChooseFileController:(ChooseFileController*)controller {
  self = [super init];
  if (self) {
    CHECK(controller);
    _chooseFileController = controller;
    _chooseFileControllerObserverBridge =
        std::make_unique<ChooseFileControllerObserverBridge>(self);
    _chooseFileControllerObservation = std::make_unique<base::ScopedObservation<
        ChooseFileController, ChooseFileController::Observer>>(
        _chooseFileControllerObserverBridge.get());
    _chooseFileControllerObservation->Observe(controller);
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  ChooseFileController* chooseFileController = _chooseFileController;
  // Stopping observation first so that submitting the selection will not
  // trigger observer calls.
  _chooseFileControllerObservation.reset();
  _chooseFileControllerObserverBridge.reset();
  _chooseFileController = nullptr;
  if (chooseFileController && !chooseFileController->HasSubmittedSelection()) {
    // If the controller still exists when the UI is being disconnect, cancel
    // the selection.
    chooseFileController->SubmitSelection(nil, nil, nil);
  }
}

#pragma mark - ChooseFileControllerObserving

- (void)chooseFileControllerDestroyed:(ChooseFileController*)controller {
  _chooseFileControllerObservation.reset();
  _chooseFileControllerObserverBridge.reset();
  _chooseFileController = nullptr;
  [self.fileUploadPanelHandler hideFileUploadPanel];
}

@end
