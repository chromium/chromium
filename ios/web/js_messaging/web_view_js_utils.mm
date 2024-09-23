// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_js_utils.h"

#import <CoreFoundation/CoreFoundation.h>
#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/debug/crash_logging.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"

namespace {

// Converts result of WKWebView script evaluation to base::Value, parsing
// `wk_result` up to a depth of `max_depth`.
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
    base::Value::Dict dictionary;
    for (id key in wk_result) {
      NSString* obj_c_string = base::apple::ObjCCast<NSString>(key);
      const std::string path = base::SysNSStringToUTF8(obj_c_string);
      SCOPED_CRASH_KEY_STRING32("ScriptMessage", "path", path);
      std::unique_ptr<base::Value> value =
          ValueResultFromWKResult(wk_result[obj_c_string], max_depth - 1);
      if (value) {
        dictionary.SetByDottedPath(
            path, base::Value::FromUniquePtrValue(std::move(value)));
      }
    }
    result = std::make_unique<base::Value>(std::move(dictionary));
  } else if (result_type == CFArrayGetTypeID()) {
    base::Value::List list;
    for (id list_item in wk_result) {
      std::unique_ptr<base::Value> value =
          ValueResultFromWKResult(list_item, max_depth - 1);
      if (value) {
        list.Append(base::Value::FromUniquePtrValue(std::move(value)));
      }
    }
    result = std::make_unique<base::Value>(std::move(list));
  } else {
    NOTREACHED_IN_MIGRATION();  // Convert other types as needed.
  }
  return result;
}

// Converts base::Value to an equivalent Foundation object, parsing,
// `value_result` up to a depth of `max_depth`.
id NSObjectFromValueResult(const base::Value* value_result, int max_depth) {
  if (!value_result) {
    return nil;
  }

  id result = nil;

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  if (value_result->is_string()) {
    result = base::SysUTF8ToNSString(value_result->GetString());
    DCHECK([result isKindOfClass:[NSString class]]);
  } else if (value_result->is_int()) {
    result = [NSNumber numberWithInt:value_result->GetInt()];
    DCHECK([result isKindOfClass:[NSNumber class]]);
  } else if (value_result->is_double()) {
    result = [NSNumber numberWithDouble:value_result->GetDouble()];
    DCHECK([result isKindOfClass:[NSNumber class]]);
  } else if (value_result->is_bool()) {
    result = [NSNumber numberWithBool:value_result->GetBool()];
    DCHECK([result isKindOfClass:[NSNumber class]]);
  } else if (value_result->is_none()) {
    result = [NSNull null];
    DCHECK([result isKindOfClass:[NSNull class]]);
  } else if (value_result->is_dict()) {
    NSMutableDictionary* dictionary = [[NSMutableDictionary alloc] init];
    for (const auto pair : value_result->GetDict()) {
      NSString* key = base::SysUTF8ToNSString(pair.first);
      id wk_result = NSObjectFromValueResult(&pair.second, max_depth - 1);
      if (wk_result) {
        [dictionary setValue:wk_result forKey:key];
      }
    }
    result = [dictionary copy];
  } else if (value_result->is_list()) {
    NSMutableArray* array = [[NSMutableArray alloc] init];
    for (const base::Value& value : value_result->GetList()) {
      id wk_result = NSObjectFromValueResult(&value, max_depth - 1);
      if (wk_result) {
        [array addObject:wk_result];
      }
    }
    result = [array copy];
  } else {
    NOTREACHED_IN_MIGRATION();  // Convert other types as needed.
  }
  return result;
}

// Runs completion_handler with an error representing that no web view currently
// exists so Javascript could not be executed.
void NotifyCompletionHandlerNullWebView(void (^completion_handler)(id,
                                                                   NSError*)) {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* error_message =
        @"JS evaluation failed because there is no web view.";
    NSError* error = [[NSError alloc]
        initWithDomain:web::kJSEvaluationErrorDomain
                  code:web::JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW
              userInfo:@{NSLocalizedDescriptionKey : error_message}];
    completion_handler(nil, error);
  });
}

}  // namespace

namespace web {

NSString* const kJSEvaluationErrorDomain = @"JSEvaluationError";
int const kMaximumParsingRecursionDepth = 10;

std::unique_ptr<base::Value> ValueResultFromWKResult(id wk_result) {
  return ::ValueResultFromWKResult(wk_result, kMaximumParsingRecursionDepth);
}

id NSObjectFromValueResult(const base::Value* value_result) {
  return ::NSObjectFromValueResult(value_result, kMaximumParsingRecursionDepth);
}

void ExecuteJavaScript(WKWebView* web_view,
                       NSString* script,
                       void (^completion_handler)(id, NSError*)) {
  DCHECK([script length]);
  if (!web_view && completion_handler) {
    NotifyCompletionHandlerNullWebView(completion_handler);
    return;
  }

  [web_view evaluateJavaScript:script completionHandler:completion_handler];
}

void ExecuteJavaScript(WKWebView* web_view,
                       WKContentWorld* content_world,
                       WKFrameInfo* frame_info,
                       NSString* script,
                       void (^completion_handler)(id, NSError*)) {
  DCHECK(content_world);
  // `frame_info` is required to ensure `script` is executed on the correct
  // webpage. This works because a `frame_info` instance is associated with a
  // particular loaded webpage/navigation and the script execution will only
  // happen in the web view if the current frame_info matches.
  DCHECK(frame_info);

  DCHECK([script length] > 0);
  if (!web_view && completion_handler) {
    NotifyCompletionHandlerNullWebView(completion_handler);
    return;
  }

  [web_view evaluateJavaScript:script
                       inFrame:frame_info
                inContentWorld:content_world
             completionHandler:completion_handler];
}

void RegisterExistingFrames(WKWebView* web_view,
                            WKContentWorld* content_world) {
  DCHECK(content_world);

  NSString* script = @"__gCrWeb.message.getExistingFrames();";

  [web_view evaluateJavaScript:script
                       inFrame:nil
                inContentWorld:content_world
             completionHandler:nil];
}

}  // namespace web
