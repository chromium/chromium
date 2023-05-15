// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

// Test double for BrowserProviderInterface implementors. All properties are
// writeable. It behaves as follows:
// - It creates two StubBrowserProvider on init.
// - `currentBrowserProvider` is settable and defaults to the main interface.
// - The current provider is has `current` set to YES on init.
// - The incognito provider has `incognito` set to YES on init.
// - All other methods are no-ops, and there is no control logic to update the
//   interfaces in any way after init.
@interface StubBrowserProviderInterface : NSObject <BrowserProviderInterface>

// Specify concrete (stub) implementations for the interfaces, so tests can
// set values on them.
@property(nonatomic, weak, readwrite)
    StubBrowserProvider* currentBrowserProvider;
@property(nonatomic, readwrite) StubBrowserProvider* mainBrowserProvider;
@property(nonatomic, readwrite) StubBrowserProvider* incognitoBrowserProvider;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_INTERFACE_H_
