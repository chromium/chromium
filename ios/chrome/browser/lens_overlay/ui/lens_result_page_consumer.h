// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_CONSUMER_H_

#import <UIKit/UIKit.h>

/// Consumer of the lens result page.
@protocol LensResultPageConsumer

/// Sets `webView` in the consumer.
- (void)setWebView:(UIView*)webView;

/// Sets the visibility of the web view.
- (void)setWebViewHidden:(BOOL)hidden;

/// Sets the loading progress.
/// This value is bound between 0 (meaning no progress) and 1 (meaning the page
/// has fully loaded).
- (void)setLoadingProgress:(float)progress;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_CONSUMER_H_
