// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_
#define IOS_WEB_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_

#import <Foundation/Foundation.h>

@class WKContentWorld;
@class WKFrameInfo;
@class WKWebView;

namespace base {
class Value;
}  // namespace base

#import "ios/web/public/js_messaging/web_view_js_utils.h"

namespace web {

// The domain for JS evaluation NSErrors in web.
extern NSString* const kJSEvaluationErrorDomain;

// The maximum recursion depth when parsing JS results.
extern int const kMaximumParsingRecursionDepth;

// The type of errors that can occur while evaluating JS.
enum JSEvaluationErrorCode {
  // No web view present to evaluate JS.
  JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW = -1000,
  // JS evaluation is not allowed for security reasons.
  JS_EVALUATION_ERROR_CODE_REJECTED = -1001,
};

// Converts base::Value to an equivalent Foundation object.
id NSObjectFromValueResult(const base::Value* value_result);

// Executes JavaScript on WKWebView. If the web view cannot execute JS at the
// moment, `completion_handler` is called with an NSError.
void ExecuteJavaScript(WKWebView* web_view,
                       NSString* script,
                       void (^completion_handler)(id, NSError*));

// Executes JavaScript for `web_view` in `frame_info` within `content_world` and
// calls `completion_handler` with the result. `content_world` and `frame_info`
// are required. If the web view cannot execute JS at the moment,
// `completion_handler` is called with an NSError.
void ExecuteJavaScript(WKWebView* web_view,
                       WKContentWorld* content_world,
                       WKFrameInfo* frame_info,
                       NSString* script,
                       void (^completion_handler)(id, NSError*));

// Calls into the JavaScript in `content_world` to trigger the registration of
// all web frames.
// NOTE: This call is sent to the WKWebView directly, because the result of this
// call will create the WebFrames. (Thus, the WebFrames do not yet exist and
// the ExecuteJavaScript variant above requiring `frame_info` can not be used.)
void RegisterExistingFrames(WKWebView* web_view, WKContentWorld* content_world);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_
