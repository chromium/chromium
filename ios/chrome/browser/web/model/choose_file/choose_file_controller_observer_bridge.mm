// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_observer_bridge.h"

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

ChooseFileControllerObserverBridge::ChooseFileControllerObserverBridge(
    id<ChooseFileControllerObserving> observer)
    : observer_(observer) {}

ChooseFileControllerObserverBridge::~ChooseFileControllerObserverBridge() =
    default;

void ChooseFileControllerObserverBridge::ChooseFileControllerDestroyed(
    ChooseFileController* controller) {
  [observer_ chooseFileControllerDestroyed:controller];
}
