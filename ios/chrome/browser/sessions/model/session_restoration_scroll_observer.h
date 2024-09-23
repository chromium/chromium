// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SCROLL_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SCROLL_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

// A CRWWebViewScrollViewProxyObserver that invokes a closure when a scroll
// event occurs.
@interface SessionRestorationScrollObserver
    : NSObject <CRWWebViewScrollViewProxyObserver>

// The designated initializer. The `closure` will be invoked every time a
// scroll event completes. If the `closure` may become invalid, `-shutdown`
// must be called before this happens.
- (instancetype)initWithClosure:(base::RepeatingClosure)closure
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Clears the `closure`. Should be called before the `closure` becomes invalid.
// After this method is called, the `closure` won't be invoked.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SCROLL_OBSERVER_H_
