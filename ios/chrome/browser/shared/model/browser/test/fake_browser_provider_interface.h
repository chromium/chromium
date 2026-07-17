// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

// Fake implementation of `BrowserProviderInterface` for unit tests.
@interface FakeBrowserProviderInterface : NSObject <BrowserProviderInterface>

// The designated current browser provider.
@property(nonatomic, weak) id<BrowserProvider> currentBrowserProvider;

// The main (non-incognito) browser provider.
@property(nonatomic, strong) id<BrowserProvider> mainBrowserProvider;

// The incognito browser provider.
@property(nonatomic, strong) id<BrowserProvider> incognitoBrowserProvider;

// Whether an incognito browser provider is created and available.
@property(nonatomic, assign) BOOL hasIncognitoBrowserProvider;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_INTERFACE_H_
