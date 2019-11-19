// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

namespace web {

// Matcher for the WKWebView.
id<GREYMatcher> WebView();

// Matcher for WKWebView's scroll view.
id<GREYMatcher> WebViewScrollView();

// Matcher for back button in web shell.
id<GREYMatcher> BackButton();

// Matcher for forward button in web shell.
id<GREYMatcher> ForwardButton();

}  // namespace web

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_
