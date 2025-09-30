// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_NAVIGATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_NAVIGATION_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the AIM prototype's web view.
@protocol AIMPrototypeNavigationConsumer <NSObject>

// Sets a webview. Passing nil removes any existing webview from the consumer.
- (void)setWebView:(UIView*)webView;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_NAVIGATION_CONSUMER_H_
