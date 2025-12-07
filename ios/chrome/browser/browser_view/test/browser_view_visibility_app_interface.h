// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_TEST_BROWSER_VIEW_VISIBILITY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_TEST_BROWSER_VIEW_VISIBILITY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

enum class BrowserViewVisibilityState;

/// App side implementation of helpers for browser view visibility eg tests,
/// including a browser visibility state observer.
@interface BrowserViewVisibilityAppInterface : NSObject

+ (void)startObservingBrowserViewVisibilityState;

+ (void)stopObservingBrowserViewVisibilityState;

+ (BrowserViewVisibilityState)currentState;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_TEST_BROWSER_VIEW_VISIBILITY_APP_INTERFACE_H_
