// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles updates of the parent access UI.
@protocol ParentAccessConsumer

// Sets the parent access WebView configured to display web content for the
// parent access UI.
- (void)setWebView:(UIView*)view;

// Sets the visibility of the web view.
- (void)setWebViewHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_CONSUMER_H_
