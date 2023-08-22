// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_observer_bridge.h"

#import "base/check.h"

SafetyCheckObserverBridge::SafetyCheckObserverBridge(
    id<SafetyCheckManagerObserver> delegate,
    IOSChromeSafetyCheckManager* manager)
    : delegate_(delegate), safety_check_manager_(manager) {
  CHECK(delegate_);
  CHECK(safety_check_manager_);

  safety_check_manager_observation_.Observe(manager);
}

SafetyCheckObserverBridge::~SafetyCheckObserverBridge() {
  if (safety_check_manager_) {
    safety_check_manager_->RemoveObserver(this);
  }
}

void SafetyCheckObserverBridge::PasswordCheckStateChanged(
    PasswordSafetyCheckState state) {
  [delegate_ passwordCheckStateChanged:state];
}

void SafetyCheckObserverBridge::SafeBrowsingCheckStateChanged(
    SafeBrowsingSafetyCheckState state) {
  [delegate_ safeBrowsingCheckStateChanged:state];
}

void SafetyCheckObserverBridge::UpdateChromeCheckStateChanged(
    UpdateChromeSafetyCheckState state) {
  [delegate_ updateChromeCheckStateChanged:state];
}

void SafetyCheckObserverBridge::RunningStateChanged(
    RunningSafetyCheckState state) {
  [delegate_ runningStateChanged:state];
}

void SafetyCheckObserverBridge::ManagerWillShutdown(
    IOSChromeSafetyCheckManager* safety_check_manager) {
  safety_check_manager->RemoveObserver(this);
}
