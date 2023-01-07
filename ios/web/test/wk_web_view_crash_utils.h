// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WK_WEB_VIEW_CRASH_UTILS_H_
#define IOS_WEB_TEST_WK_WEB_VIEW_CRASH_UTILS_H_

#import <WebKit/WebKit.h>

namespace web {

// Simulates WKWebView crash by calling its private API.
void SimulateWKWebViewCrash(WKWebView* webView);

// Returns a specialized WKWebView mock object with overridden JavaScript
// evaluation method that fails with WKErrorWebContentProcessTerminated error.
WKWebView* BuildTerminatedWKWebView();

// Returns a specialized WKWebView mock object with overridden JavaScript
// evaluation method that always succeeds with nil result.
WKWebView* BuildHealthyWKWebView();
}  // web

#endif // IOS_WEB_TEST_WK_WEB_VIEW_CRASH_UTILS_H_
