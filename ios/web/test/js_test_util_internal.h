// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_
#define IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {
namespace test {

// Synchronously executes `script` in `content_world` and returns result.
// NOTE: Generally, tests should not deal with raw WKContentWorlds. Instead,
// prefer specifying the associated JavaScriptFeature instance using
// WebTestWithWebState::ExecuteJavaScriptForFeature.
id ExecuteJavaScript(WKWebView* web_view,
                     WKContentWorld* content_world,
                     NSString* script);

// Executes `script` in `content_world` as an asynchronous JavaScript function,
// waits for execution to complete, and returns the result.
id ExecuteAsyncJavaScript(WKWebView* web_view,
                          WKContentWorld* content_world,
                          NSString* script);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_
