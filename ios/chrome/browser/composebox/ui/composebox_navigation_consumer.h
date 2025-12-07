// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_NAVIGATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_NAVIGATION_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the composebox's web view.
@protocol ComposeboxNavigationConsumer <NSObject>

// Sets a webview. Passing nil removes any existing webview from the consumer.
- (void)setWebView:(UIView*)webView;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_NAVIGATION_CONSUMER_H_
