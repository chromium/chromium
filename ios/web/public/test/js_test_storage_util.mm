// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_storage_util.h"

#import "base/bind.h"
#import "base/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace test {

namespace {

// Convenience wrapper for web_frame.CallJavaScriptFunction that synchronously
// calls the provided function.
bool ExecuteJavaScriptInFrame(
    WebFrame* web_frame,
    const std::string& name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  __block bool completed = false;
  __block base::OnceCallback<void(const base::Value*)> block_callback =
      std::move(callback);
  web_frame->CallJavaScriptFunction(name, parameters,
                                    base::BindOnce(^(const base::Value* value) {
                                      completed = true;
                                      std::move(block_callback).Run(value);
                                    }),
                                    timeout);
  bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return completed;
      });
  return success;
}

// Convenience wrapper for web_frame.CallJavaScriptFunction that synchronously
// calls the provided function.
bool ExecuteJavaScriptInFrame(WebFrame* web_frame,
                              const std::string& name,
                              const std::vector<base::Value>& parameters) {
  return ExecuteJavaScriptInFrame(web_frame, name, parameters,
                                  base::BindOnce(^(const base::Value*){
                                  }),
                                  kWaitForJSCompletionTimeout);
}

// Saves `key`, `value` to a Javascript storage type in `web_frame` using the
// __gCrWeb function `name`. Places any error message from the JavaScript into
// `error_message`.
bool SetStorage(WebFrame* web_frame,
                const std::string& set_function,
                NSString* key,
                NSString* key_value,
                NSString** error_message) {
  __block NSString* block_error_message;
  __block bool set_success = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(base::SysNSStringToUTF8(key)));
  params.push_back(base::Value(base::SysNSStringToUTF8(key_value)));
  bool success = ExecuteJavaScriptInFrame(
      web_frame, set_function, params,
      base::BindOnce(^(const base::Value* value) {
        if (value->is_bool()) {
          set_success = value->GetBool();
        } else if (value->is_dict()) {
          block_error_message =
              base::SysUTF8ToNSString(value->FindPath("message")->GetString());
          set_success = true;
        }
      }),
      kWaitForJSCompletionTimeout);
  if (error_message) {
    *error_message = block_error_message;
  }

  return success && set_success;
}

// Reads the value for the given `key` from storage on `web_frame` using
// the __gCrWeb function `name`. The read value will be placed in `result` and
// any JavaScript error will be placed in `error_message`.
bool GetStorage(WebFrame* web_frame,
                const std::string& get_function,
                NSString* key,
                NSString** result,
                NSString** error_message) {
  __block NSString* block_result;
  __block NSString* block_error_message;
  __block bool lookup_success = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(base::SysNSStringToUTF8(key)));
  bool success = ExecuteJavaScriptInFrame(
      web_frame, get_function, params,
      base::BindOnce(^(const base::Value* value) {
        if (value->is_string()) {
          block_result = base::SysUTF8ToNSString(value->GetString());
          lookup_success = true;
        } else if (value->is_dict()) {
          block_error_message =
              base::SysUTF8ToNSString(value->FindPath("message")->GetString());
          lookup_success = true;
        } else {
          lookup_success = false;
        }
      }),
      kWaitForJSCompletionTimeout);

  if (error_message) {
    *error_message = block_error_message;
  }
  if (result) {
    *result = block_result;
  }
  return success && lookup_success;
}

// Saves `key`, `value` to a Javascript storage type in `web_frame` and
// `web_state` using the
// __gCrWeb function `name`. The storage being used must be async. Places any
// error message from the JavaScript into `error_message`.
bool SetAsyncStorage(WebFrame* web_frame,
                     WebState* web_state,
                     const std::string& set_function,
                     NSString* key,
                     NSString* value,
                     NSString** error_message) {
  // The test injected javascript will send a message
  // when the async is done, so listen for that here.
  __block bool async_success = false;
  __block NSString* block_error_message;
  base::CallbackListSubscription subscription_ =
      web_state->AddScriptCommandCallback(
          base::BindRepeating(^(const base::Value& message,
                                const GURL& page_url, bool user_is_interacting,
                                web::WebFrame* sender_frame) {
            const base::Value* result = message.FindKey("result");
            if (!result) {
              return;
            }
            if (result->is_bool()) {
              async_success = result->GetBool();
              return;
            }
            if (result->is_dict()) {
              const std::string* messageString =
                  result->FindStringKey("message");
              if (messageString) {
                block_error_message = base::SysUTF8ToNSString(*messageString);
                async_success = true;
                return;
              }
            }
            async_success = false;
          }),
          "cookieTest");

  if (!SetStorage(web_frame, set_function, key, value, nil)) {
    return false;
  }

  bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return async_success;
      });

  if (error_message) {
    *error_message = block_error_message;
  }

  return success;
}

// Reads the value for the given `key` from storage on `web_frame` using
// the __gCrWeb function `name`. The storage type must be async. The read value
// will be placed in `result` and any JavaScript error will be placed in
// `error_message`.
bool GetAsyncStorage(WebFrame* web_frame,
                     WebState* web_state,
                     const std::string& get_function,
                     NSString* key,
                     NSString** result,
                     NSString** error_message) {
  // The test injected javascript will send a message
  // when the async is done, so listen for that here.
  __block bool async_success = false;
  __block NSString* block_result;
  __block NSString* block_error_message;
  base::CallbackListSubscription subscription_ =
      web_state->AddScriptCommandCallback(
          base::BindRepeating(^(const base::Value& message,
                                const GURL& page_url, bool user_is_interacting,
                                web::WebFrame* sender_frame) {
            const base::Value* resultValue = message.FindPath("result");
            if (!resultValue) {
              return;
            }
            if (resultValue->is_string()) {
              block_result = base::SysUTF8ToNSString(resultValue->GetString());
              async_success = true;
              return;
            }
            if (resultValue->is_dict()) {
              const std::string* messageStr =
                  resultValue->FindStringKey("message");
              if (messageStr) {
                block_error_message = base::SysUTF8ToNSString(*messageStr);
                async_success = true;
                return;
              }
            }

            async_success = false;
          }),
          "cookieTest");

  if (!GetStorage(web_frame, get_function, key, nil, nil)) {
    return false;
  }

  bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return async_success;
      });

  if (result) {
    *result = block_result;
  }
  if (error_message) {
    *error_message = block_error_message;
  }

  return success;
}

}  // namespace

bool SetCookie(WebFrame* web_frame, NSString* key, NSString* value) {
  std::vector<base::Value> params;
  NSString* cookie = [NSString
      stringWithFormat:@"%@=%@; Expires=Tue, 05-May-9999 02:18:23 GMT; Path=/",
                       key, value];
  params.push_back(base::Value(base::SysNSStringToUTF8(cookie)));
  return ExecuteJavaScriptInFrame(web_frame, "cookieTest.setCookie", params);
}

bool GetCookies(WebFrame* web_frame, NSString** cookies) {
  __block NSString* result = nil;
  std::vector<base::Value> params;
  bool success = ExecuteJavaScriptInFrame(
      web_frame, "cookieTest.getCookies", params,
      base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_string());
        result = base::SysUTF8ToNSString(value->GetString());
      }),
      kWaitForJSCompletionTimeout);
  if (cookies) {
    *cookies = result;
  }
  return success;
}

bool SetLocalStorage(WebFrame* web_frame,
                     NSString* key,
                     NSString* value,
                     NSString** error_message) {
  return SetStorage(web_frame, "cookieTest.setLocalStorage", key, value,
                    error_message);
}

bool GetLocalStorage(WebFrame* web_frame,
                     NSString* key,
                     NSString** result,
                     NSString** error_message) {
  return GetStorage(web_frame, "cookieTest.getLocalStorage", key, result,
                    error_message);
}

bool SetSessionStorage(WebFrame* web_frame,
                       NSString* key,
                       NSString* value,
                       NSString** error_message) {
  return SetStorage(web_frame, "cookieTest.setSessionStorage", key, value,
                    error_message);
}

bool GetSessionStorage(WebFrame* web_frame,
                       NSString* key,
                       NSString** result,
                       NSString** error_message) {
  return GetStorage(web_frame, "cookieTest.getSessionStorage", key, result,
                    error_message);
}

bool SetCache(WebFrame* web_frame,
              WebState* web_state,
              NSString* key,
              NSString* value,
              NSString** error_message) {
  return SetAsyncStorage(web_frame, web_state, "cookieTest.setCache", key,
                         value, error_message);
}

bool GetCache(WebFrame* web_frame,
              WebState* web_state,
              NSString* key,
              NSString** result,
              NSString** error_message) {
  return GetAsyncStorage(web_frame, web_state, "cookieTest.getCache", key,
                         result, error_message);
}

bool SetIndexedDB(WebFrame* web_frame,
                  WebState* web_state,
                  NSString* key,
                  NSString* value,
                  NSString** error_message) {
  return SetAsyncStorage(web_frame, web_state, "cookieTest.setIndexedDB", key,
                         value, error_message);
}

bool GetIndexedDB(WebFrame* web_frame,
                  WebState* web_state,
                  NSString* key,
                  NSString** result,
                  NSString** error_message) {
  return GetAsyncStorage(web_frame, web_state, "cookieTest.getIndexedDB", key,
                         result, error_message);
}

}  // namespace test
}  // namespace web
