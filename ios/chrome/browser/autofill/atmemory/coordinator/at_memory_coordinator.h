// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Main coordinator for the AtMemory feature flow. It manages the shared
// navigation controller and coordinates transitions between the search and
// granular fill child coordinators.
@interface AtMemoryCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_
