// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_STORE_KIT_LAUNCHER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_STORE_KIT_LAUNCHER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/store_kit/store_kit_launcher.h"

// Implementation of StoreKitLauncher that whenever openAppStore with productID
// is called, launchedProductID will be set to that productID. Users need to
// reset `launchedProductID` between uses.
@interface FakeStoreKitLauncher : NSObject<StoreKitLauncher>
// This string will have the product id that store kit with launched for, if
// openAppStore was called with string ID.
@property(nonatomic, copy) NSString* launchedProductID;

// This dictionary will have the product params that store kit with launched
// with, if openAppStoreWithParams was called with parameters dictionary.
@property(nonatomic, copy) NSDictionary* launchedProductParams;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_STORE_KIT_LAUNCHER_H_
