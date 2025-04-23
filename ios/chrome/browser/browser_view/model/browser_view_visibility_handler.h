// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_HANDLER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_HANDLER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_audience.h"

using BrowserViewVisibilityStateChangeCallback =
    base::RepeatingCallback<void(/*current=*/BrowserViewVisibilityState,
                                 /*previous=*/BrowserViewVisibilityState)>;

/// Objective-C helper for `BrowserViewVisibilityNotifierBrowserAgent` that
/// implements the `BrowserViewVisibilityAudience` protocol.
@interface BrowserViewVisibilityHandler
    : NSObject <BrowserViewVisibilityAudience>

/// Designated initializer.
- (instancetype)initWithVisibilityChangeCallback:
    (const BrowserViewVisibilityStateChangeCallback&)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_HANDLER_H_
