// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser_provider.h"

class Browser;

// Test double for BrowserProvider implementors. All properties are writable,
// and have nil, nullptr, or NO as default values.
@interface StubBrowserProvider : NSObject <BrowserProvider>

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Must be called before the Browser is destroyed.
- (void)shutdown;

@property(nonatomic, readwrite) Browser* browser;
- (UIViewController*)viewController:(BrowserProviderPassKey)key;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_H_
