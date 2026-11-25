// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_DEFERRED_FIRST_RUN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_DEFERRED_FIRST_RUN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

// Coordinator that sequentially presents First Run screens that were deferred
// during First Run via `prefs::kFirstRunDeferredScreens`. Sends
// `-hideDeferredFirstRunPromo` to the `PromosManagerCommands` handler when the
// flow finishes or is dismissed and the coordinator wants to be stopped.
@interface DeferredFirstRunCoordinator : ChromeCoordinator

// Promos Manager UI handler notified when the deferred flow is dismissed.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_DEFERRED_FIRST_RUN_COORDINATOR_H_
