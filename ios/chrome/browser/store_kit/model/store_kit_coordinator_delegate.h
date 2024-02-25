// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_DELEGATE_H_

@class StoreKitCoordinator;

// Delegate for the store kit coordinator.
@protocol StoreKitCoordinatorDelegate <NSObject>

// Requests the delegate to stop the coordinator.
- (void)storeKitCoordinatorWantsToStop:(StoreKitCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_DELEGATE_H_
