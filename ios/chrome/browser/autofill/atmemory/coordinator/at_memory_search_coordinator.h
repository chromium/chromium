// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/common/unique_ids.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AtMemoryFillCommands;
@protocol AtMemorySearchResultCommands;
@protocol AutofillSettingsNavigator;

// Child coordinator for the AtMemory search UI. Managed by AtMemoryCoordinator,
// it is the main UI to handle typing, displaying results, showing notices, and
// handling errors.
@interface AtMemorySearchCoordinator : ChromeCoordinator

// Handler for filling commands.
@property(nonatomic, weak) id<AtMemoryFillCommands> fillHandler;

// Handler for search result commands.
@property(nonatomic, weak) id<AtMemorySearchResultCommands> searchResultHandler;

// Navigator used to open Autofill settings pages.
@property(nonatomic, weak) id<AutofillSettingsNavigator> settingsNavigator;

// Initializes the coordinator. `navigationController` is the base navigation
// controller used to present the search UI. `browser` provides access to
// profile-keyed services. `fieldId` specifies the focused field that initiated
// AtMemory.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                         fieldId:
                                             (autofill::FieldGlobalId)fieldId
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_COORDINATOR_H_
