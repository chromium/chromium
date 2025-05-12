// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"

class HomeBackgroundCustomizationService;

// Observes HomeBackgroundCustomizationService events in Objective-C.
@protocol HomeBackgroundCustomizationServiceObserving <NSObject>

// Called when the background changes.
- (void)onBackgroundChanged;

@end

// Bridge to observe HomeBackgroundCustomizationService in Objective-C.
class HomeBackgroundCustomizationServiceObserverBridge
    : public HomeBackgroundCustomizationServiceObserver {
 public:
  explicit HomeBackgroundCustomizationServiceObserverBridge(
      HomeBackgroundCustomizationService* service,
      id<HomeBackgroundCustomizationServiceObserving> observer);

  HomeBackgroundCustomizationServiceObserverBridge(
      const HomeBackgroundCustomizationServiceObserverBridge&) = delete;
  HomeBackgroundCustomizationServiceObserverBridge& operator=(
      const HomeBackgroundCustomizationServiceObserverBridge&) = delete;

  ~HomeBackgroundCustomizationServiceObserverBridge() override;

  // HomeBackgroundCustomizationServiceObserver:
  void OnBackgroundChanged() override;

 private:
  __weak id<HomeBackgroundCustomizationServiceObserving> observer_ = nil;
  base::ScopedObservation<HomeBackgroundCustomizationService,
                          HomeBackgroundCustomizationServiceObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_BRIDGE_H_
