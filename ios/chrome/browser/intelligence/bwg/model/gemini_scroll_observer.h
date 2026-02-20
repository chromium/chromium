// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SCROLL_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SCROLL_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

// Observes scroll events and executes a callback.
@interface GeminiScrollObserver : NSObject <CRWWebViewScrollViewProxyObserver>

// Initializes the observer with a callback to be executed on scroll events.
- (instancetype)initWithScrollCallback:(base::RepeatingClosure)callback;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SCROLL_OBSERVER_H_
