// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer_bridge.h"

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

HomeBackgroundCustomizationServiceObserverBridge::
    HomeBackgroundCustomizationServiceObserverBridge(
        HomeBackgroundCustomizationService* service,
        id<HomeBackgroundCustomizationServiceObserving> observer)
    : observer_(observer) {
  scoped_observation_.Observe(service);
}

HomeBackgroundCustomizationServiceObserverBridge::
    ~HomeBackgroundCustomizationServiceObserverBridge() = default;

void HomeBackgroundCustomizationServiceObserverBridge::OnBackgroundChanged() {
  [observer_ onBackgroundChanged];
}
