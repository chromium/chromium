// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

// Observes ChooseFileController events from Objective-C. To use as a
// ChooseFileController::Observer, wrap in a ChooseFileControllerObserverBridge.
@protocol ChooseFileControllerObserving <NSObject>

// Called when `controller` is being destroyed;
- (void)chooseFileControllerDestroyed:(ChooseFileController*)controller;

@end

// Bridge to use an `id<ChooseFileControllerObserving>` as a
// `ChooseFileController::Observer`.
class ChooseFileControllerObserverBridge
    : public ChooseFileController::Observer {
 public:
  explicit ChooseFileControllerObserverBridge(
      id<ChooseFileControllerObserving> observer);

  ChooseFileControllerObserverBridge(
      const ChooseFileControllerObserverBridge&) = delete;
  ChooseFileControllerObserverBridge& operator=(
      const ChooseFileControllerObserverBridge&) = delete;

  ~ChooseFileControllerObserverBridge() override;

  // ChooseFileController::Observer methods.
  void ChooseFileControllerDestroyed(ChooseFileController* controller) override;

 private:
  __weak id<ChooseFileControllerObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_OBSERVER_BRIDGE_H_
