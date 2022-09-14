// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_
#define IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_


#import <Foundation/Foundation.h>

@protocol GREYMatcher;

namespace web {

class WebState;

// Matcher for WKWebView which belogs to the given `webState`.
id<GREYMatcher> WebViewInWebState(WebState* web_state);

// Matcher for WKWebView's scroll view.
id<GREYMatcher> WebViewScrollView(WebState* web_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_
