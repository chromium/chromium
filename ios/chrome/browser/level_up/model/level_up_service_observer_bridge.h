// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/level_up/model/level_up_service_observer.h"

class LevelUpService;

// Observes `LevelUpService` events and notifies the Objective-C delegate of
// changes.
@protocol LevelUpServiceObserving <NSObject>

// Notifies the observer that the level and pref have changed.
- (void)levelUpServiceDidCompleteTask:(LevelUpService*)service;

// Notifies the observer that a stat value has changed.
- (void)levelUpServiceDidUpdateStats:(LevelUpService*)service;

@end

// Bridge to observe `LevelUpService` in Objective-C
class LevelUpServiceObserverBridge : public LevelUpServiceObserver {
 public:
  explicit LevelUpServiceObserverBridge(LevelUpService* service,
                                        id<LevelUpServiceObserving> observer);
  LevelUpServiceObserverBridge(const LevelUpServiceObserverBridge&) = delete;
  LevelUpServiceObserverBridge& operator=(const LevelUpServiceObserverBridge&) =
      delete;
  ~LevelUpServiceObserverBridge() override;

  // LevelUpServiceObserver overrides
  void OnTaskCompleted() override;
  void OnStatValueChanged() override;

 private:
  __weak id<LevelUpServiceObserving> observer_ = nil;
  raw_ptr<LevelUpService> service_ = nullptr;
  base::ScopedObservation<LevelUpService, LevelUpServiceObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_BRIDGE_H_
