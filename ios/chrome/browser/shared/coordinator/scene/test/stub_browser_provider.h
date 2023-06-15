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
@property(nonatomic, readwrite) UIViewController* viewController;
@property(nonatomic, readwrite) Browser* browser;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_STUB_BROWSER_PROVIDER_H_
