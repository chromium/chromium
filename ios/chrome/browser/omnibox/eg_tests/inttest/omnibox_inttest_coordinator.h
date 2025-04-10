// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class OmniboxCoordinator;

/// Coordinator that presents omnibox UI elements for integration tests.
@interface OmniboxInttestCoordinator : ChromeCoordinator

/// The omnibox coordinator.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;

/// Simulates the active page being NTP.
- (void)simulateNTP;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_
