// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

// Coordinator for presenting the ephemeral theme promo on the New Tab Page.
@interface HomeCustomizationEphemeralThemePromoCoordinator : ChromeCoordinator

// Promos manager UI handler to alert for promo dismissal.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_COORDINATOR_H_
