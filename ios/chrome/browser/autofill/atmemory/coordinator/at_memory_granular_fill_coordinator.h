// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
struct Suggestion;
}

@protocol AtMemoryFillCommands;

// Child coordinator for the AtMemory granular fill UI. Managed by
// AtMemoryCoordinator, it displays detailed fields for a selected
// AtMemoryResult item inside the shared navigation controller.
@interface AtMemoryGranularFillCoordinator : ChromeCoordinator

// Handler for fill commands.
@property(nonatomic, weak) id<AtMemoryFillCommands> fillHandler;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                      suggestion:(const autofill::Suggestion&)
                                                     suggestion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_COORDINATOR_H_
