// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser_provider.h"

// Fake implementation of `BrowserProvider` for unit tests.
@interface FakeBrowserProvider : NSObject <BrowserProvider>

// The active browser returned by this provider.
@property(nonatomic, assign) Browser* browser;

// The view controller returned by this provider when requested.
@property(nonatomic, strong) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_FAKE_BROWSER_PROVIDER_H_
