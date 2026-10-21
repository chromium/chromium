// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service_observer_bridge.h"

#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_observer.h"

LevelUpServiceObserverBridge::LevelUpServiceObserverBridge(
    LevelUpService* service,
    id<LevelUpServiceObserving> observer)
    : observer_(observer), service_(service) {
  CHECK(observer_);
  CHECK(service_);
  scoped_observation_.Observe(service);
}

LevelUpServiceObserverBridge::~LevelUpServiceObserverBridge() = default;

void LevelUpServiceObserverBridge::OnTaskCompleted() {
  [observer_ levelUpServiceDidCompleteTask:service_];
}

void LevelUpServiceObserverBridge::OnStatValueChanged() {
  [observer_ levelUpServiceDidUpdateStats:service_];
}
