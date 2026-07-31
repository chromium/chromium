// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AtMemorySearchResultCommands;

// Child coordinator for the AtMemory search UI. Managed by AtMemoryCoordinator,
// it is the main UI to handle typing, displaying results, showing notices, and
// handling errors.
@interface AtMemorySearchCoordinator : ChromeCoordinator

// Handler for search result commands.
@property(nonatomic, weak) id<AtMemorySearchResultCommands> searchResultHandler;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_
