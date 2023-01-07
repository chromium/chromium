// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_APP_INTERFACE_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

// Helper class to return matchers for EG tests.  These helpers are compiled
// into the app binary and can be called from either app or test code.
@interface ShellMatchersAppInterface : NSObject

// Matcher for the WKWebView.
+ (id<GREYMatcher>)webView;

// Matcher for WKWebView's scroll view.
+ (id<GREYMatcher>)webViewScrollView;

// Matcher for back button in web shell.
+ (id<GREYMatcher>)backButton;

// Matcher for forward button in web shell.
+ (id<GREYMatcher>)forwardButton;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_APP_INTERFACE_H_
