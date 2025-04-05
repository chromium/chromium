// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace ios::provider {
enum class AddToCalendarIntegrationProvider;
}  // namespace ios::provider

// The coordinator for the Enhanced Calendar feature's UI.
@interface EnhancedCalendarCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// `integrationProvider` is the "add to calendar" integration provider to be
// shown to the user.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       integrationProvider:
                           (ios::provider::AddToCalendarIntegrationProvider)
                               integrationProvider NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_COORDINATOR_H_
