// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_js_utils.h"

#include <CoreFoundation/CoreFoundation.h>
#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Converts result of WKWebView script evaluation to base::Value, parsing
// |wk_result| up to a depth of |max_depth|.
std::unique_ptr<base::Value> ValueResultFromWKResult(id wk_result,
                                                     int max_depth) {
  if (!wk_result)
    return nullptr;

  std::unique_ptr<base::Value> result;

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  CFTypeID result_type = CFGetTypeID((__bridge CFTypeRef)wk_result);
  if (result_type == CFStringGetTypeID()) {
    result.reset(new base::Value(base::SysNSStringToUTF16(wk_result)));
    DCHECK(result->is_string());
  } else if (result_type == CFNumberGetTypeID()) {
    result.reset(new base::Value([wk_result doubleValue]));
    DCHECK(result->is_double());
  } else if (result_type == CFBooleanGetTypeID()) {
    result.reset(new base::Value(static_cast<bool>([wk_result boolValue])));
    DCHECK(result->is_bool());
  } else if (result_type == CFNullGetTypeID()) {
    result = std::make_unique<base::Value>();
    DCHECK(result->is_none());
  } else if (result_type == CFDictionaryGetTypeID()) {
    std::unique_ptr<base::DictionaryValue> dictionary =
        std::make_unique<base::DictionaryValue>();
    for (id key in wk_result) {
      NSString* obj_c_string = base::mac::ObjCCast<NSString>(key);
      const std::string path = base::SysNSStringToUTF8(obj_c_string);
      std::unique_ptr<base::Value> value =
          ValueResultFromWKResult(wk_result[obj_c_string], max_depth - 1);
      if (value) {
        dictionary->Set(path, std::move(value));
      }
    }
    result = std::move(dictionary);
  } else if (result_type == CFArrayGetTypeID()) {
    std::unique_ptr<base::ListValue> list = std::make_unique<base::ListValue>();
    for (id list_item in wk_result) {
      std::unique_ptr<base::Value> value =
          ValueResultFromWKResult(list_item, max_depth - 1);
      if (value) {
        list->Append(std::move(value));
      }
    }
    result = std::move(list);
  } else {
    NOTREACHED();  // Convert other types as needed.
  }
  return result;
}

}  // namespace

namespace web {

NSString* const kJSEvaluationErrorDomain = @"JSEvaluationError";
int const kMaximumParsingRecursionDepth = 10;

std::unique_ptr<base::Value> ValueResultFromWKResult(id wk_result) {
  return ::ValueResultFromWKResult(wk_result, kMaximumParsingRecursionDepth);
}

void ExecuteJavaScript(WKWebView* web_view,
                       NSString* script,
                       void (^completion_handler)(id, NSError*)) {
  DCHECK([script length]);
  if (!web_view && completion_handler) {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSString* error_message =
          @"JS evaluation failed because there is no web view.";
      NSError* error = [[NSError alloc]
          initWithDomain:kJSEvaluationErrorDomain
                    code:JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW
                userInfo:@{NSLocalizedDescriptionKey : error_message}];
      completion_handler(nil, error);
    });
    return;
  }

  [web_view evaluateJavaScript:script completionHandler:completion_handler];
}

}  // namespace web
