// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol StoreKitCoordinatorDelegate;

// Coordinates presentation of SKStoreProductViewController.
@interface StoreKitCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<StoreKitCoordinatorDelegate> delegate;

// iTunes store item product parameters dictionary. At least
// SKStoreProductParameterITunesItemIdentifier key needs to be specified, all
// other keys are optional. Must be set before starting the coordinator.
@property(nonatomic, copy) NSDictionary* iTunesProductParameters;

@end

#endif  // IOS_CHROME_BROWSER_STORE_KIT_MODEL_STORE_KIT_COORDINATOR_H_
