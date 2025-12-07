// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class FakeSuggestionsBuilder;
class GURL;
@class OmniboxCoordinator;

/// Coordinator that presents omnibox UI elements for integration tests.
@interface OmniboxInttestCoordinator : ChromeCoordinator

/// The omnibox coordinator.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;

/// Whether fake suggestions are enabled. Fake suggestions stubs autocomplete
/// controller suggestions and returns suggestions build using the
/// `fakeSuggestionsBuilder`. Disabled by default.
@property(nonatomic, assign, getter=isFakeSuggestionEnabled)
    BOOL fakeSuggestionEnabled;

/// Returns the fake suggestions builder, used to add fake suggestions in the
/// omnibox.
- (FakeSuggestionsBuilder*)fakeSuggestionsBuilder;

/// Simulates the active page being NTP. Default on start.
- (void)simulateNTP;

/// Returns the latest URL loaded by the omnibox.
- (GURL)lastURLLoaded;

/// Resets the latest URL loaded by the omnibox.
- (void)resetLastURLLoaded;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_COORDINATOR_H_
