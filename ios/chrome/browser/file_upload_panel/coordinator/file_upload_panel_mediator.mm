// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"

#import "base/check.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

@implementation FileUploadPanelMediator {
  raw_ptr<ChooseFileController> _chooseFileController;
}

#pragma mark - Initialization

- (instancetype)initWithChooseFileController:(ChooseFileController*)controller {
  self = [super init];
  if (self) {
    CHECK(controller);
    _chooseFileController = controller;
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  ChooseFileController* chooseFileController = _chooseFileController;
  _chooseFileController = nullptr;
  if (chooseFileController && !chooseFileController->HasSubmittedSelection()) {
    // If the controller still exists when the UI is being disconnect, cancel
    // the selection.
    chooseFileController->SubmitSelection(nil, nil, nil);
  }
}

@end
