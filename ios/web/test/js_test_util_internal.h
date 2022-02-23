// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_
#define IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {
namespace test {

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
// Synchronously executes |script| in |content_world| and returns result.
// NOTE: Generally, tests should not deal with raw WKContentWorlds. Instead,
// prefer specifying the associated JavaScriptFeature instance using
// WebTestWithWebState::ExecuteJavaScriptForFeature.
id ExecuteJavaScript(WKWebView* web_view,
                     WKContentWorld* content_world,
                     NSString* script) API_AVAILABLE(ios(14.0));
#endif  // defined(__IPHONE14_0)

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_TEST_JS_TEST_UTIL_INTERNAL_H_
