// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

// Test double for BrowserProviderInterface implementors. All properties are
// writable, and `currentBrowser` defaults to `mainBrowser`.
@interface StubBrowserProviderInterface : NSObject <BrowserProviderInterface>

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser
               incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Must be called before the Browser instances are destroyed.
- (void)shutdown;

// Redefined as readwrite for testing.
@property(nonatomic, readwrite) Browser* currentBrowser;
@property(nonatomic, readwrite) Browser* mainBrowser;
@property(nonatomic, readwrite) Browser* incognitoBrowser;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_
