// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frame_impl.h"

#import <Foundation/Foundation.h>

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/json/json_writer.h"
#import "base/logging.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_content_world.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "url/gurl.h"

namespace {

// Creates a JavaScript string for executing the function __gCrWeb.`name` with
// `parameters`.
NSString* CreateFunctionCallWithParamaters(
    const std::string& name,
    const base::Value::List& parameters) {
  NSMutableArray* parameter_strings = [[NSMutableArray alloc] init];
  for (const auto& value : parameters) {
    std::string string_value;
    base::JSONWriter::Write(value, &string_value);
    [parameter_strings addObject:base::SysUTF8ToNSString(string_value)];
  }

  return [NSString
      stringWithFormat:@"__gCrWeb.%s(%@)", name.c_str(),
                       [parameter_strings componentsJoinedByString:@","]];
}

// The NSError message returned for frames which can not execute JavaScript.
// This string is used to filter these errors because they share a more general
// error code `WKErrorJavaScriptExceptionOccurred`.
const NSString* kCannotExecuteJSInDocumentErrorMessage =
    @"Cannot execute JavaScript in this document";

}  // namespace

namespace web {

const double kJavaScriptFunctionCallDefaultTimeout = 100.0;

WebFrameImpl::WebFrameImpl(WKFrameInfo* frame_info,
                           const std::string& frame_id,
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_info_(frame_info),
      frame_id_(base::ToLowerASCII(frame_id)),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state) {
  DCHECK(frame_info_);
  DCHECK(web_state_);
  web_state->AddObserver(this);
}

WebFrameImpl::~WebFrameImpl() {
  CancelPendingRequests();
  DetachFromWebState();
}

WebFrameInternal* WebFrameImpl::GetWebFrameInternal() {
  return this;
}

WebState* WebFrameImpl::GetWebState() {
  return web_state_;
}

std::string WebFrameImpl::GetFrameId() const {
  return frame_id_;
}

bool WebFrameImpl::IsMainFrame() const {
  return is_main_frame_;
}

GURL WebFrameImpl::GetSecurityOrigin() const {
  return security_origin_;
}

BrowserState* WebFrameImpl::GetBrowserState() {
  return GetWebState()->GetBrowserState();
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world,
    bool reply_with_result) {
  int message_id = next_message_id_;
  next_message_id_++;

  if (content_world && content_world->GetWKContentWorld()) {
    return ExecuteJavaScriptFunction(content_world, name, parameters,
                                     message_id, reply_with_result);
  }

  return false;
}

bool WebFrameImpl::CallJavaScriptFunction(const std::string& name,
                                          const base::Value::List& parameters) {
  JavaScriptContentWorld* content_world =
      JavaScriptFeatureManager::GetPageContentWorldForBrowserState(
          GetBrowserState());

  return CallJavaScriptFunctionInContentWorld(name, parameters, content_world,
                                              /*reply_with_result=*/false);
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world) {
  return CallJavaScriptFunctionInContentWorld(name, parameters, content_world,
                                              /*reply_with_result=*/false);
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const base::Value::List& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  JavaScriptContentWorld* content_world =
      JavaScriptFeatureManager::GetPageContentWorldForBrowserState(
          GetBrowserState());
  return CallJavaScriptFunctionInContentWorld(name, parameters, content_world,
                                              std::move(callback), timeout);
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  int message_id = next_message_id_;

  auto timeout_callback = std::make_unique<TimeoutCallback>(base::BindOnce(
      &WebFrameImpl::CancelRequest, base::Unretained(this), message_id));
  auto callbacks = std::make_unique<struct RequestCallbacks>(
      std::move(callback), std::move(timeout_callback));
  pending_requests_[message_id] = std::move(callbacks);

  web::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, pending_requests_[message_id]->timeout_callback->callback(),
      timeout);
  bool called =
      CallJavaScriptFunctionInContentWorld(name, parameters, content_world,
                                           /*reply_with_result=*/true);
  if (!called) {
    // Remove callbacks if the call failed.
    auto request = pending_requests_.find(message_id);
    if (request != pending_requests_.end()) {
      pending_requests_.erase(request);
    }
  }
  return called;
}

bool WebFrameImpl::ExecuteJavaScript(const std::u16string& script) {
  return ExecuteJavaScript(script,
                           base::DoNothingAs<void(const base::Value*)>());
}

bool WebFrameImpl::ExecuteJavaScript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  ExecuteJavaScriptCallbackWithError callback_with_error =
      ExecuteJavaScriptCallbackAdapter(std::move(callback));

  return ExecuteJavaScript(script, std::move(callback_with_error));
}

bool WebFrameImpl::ExecuteJavaScript(
    const std::u16string& script,
    ExecuteJavaScriptCallbackWithError callback) {
  JavaScriptContentWorld* content_world =
      JavaScriptFeatureManager::GetPageContentWorldForBrowserState(
          GetBrowserState());

  return ExecuteJavaScriptInContentWorld(script, content_world,
                                         std::move(callback));
}

base::WeakPtr<WebFrame> WebFrameImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WebFrameImpl::ExecuteJavaScriptInContentWorld(
    const std::u16string& script,
    JavaScriptContentWorld* content_world,
    ExecuteJavaScriptCallbackWithError callback) {
  DCHECK(frame_info_);

  NSString* ns_script = base::SysUTF16ToNSString(script);
  __block auto internal_callback = std::move(callback);
  void (^completion_handler)(id, NSError*) = ^void(id value, NSError* error) {
    if (error) {
      LogScriptWarning(ns_script, error);
      std::move(internal_callback).Run(nullptr, error);
    } else {
      std::move(internal_callback)
          .Run(ValueResultFromWKResult(value).get(), nil);
    }
  };

  web::ExecuteJavaScript(frame_info_.webView,
                         content_world->GetWKContentWorld(), frame_info_,
                         ns_script, completion_handler);
  return true;
}

ExecuteJavaScriptCallbackWithError
WebFrameImpl::ExecuteJavaScriptCallbackAdapter(
    base::OnceCallback<void(const base::Value*)> callback) {
  // Because blocks treat scoped-variables
  // as const, we have to redefine the callback with the
  // __block keyword to be able to run the callback inside
  // the completion handler.
  __block auto internal_callback = std::move(callback);
  return base::BindOnce(^(const base::Value* value, NSError* error) {
    if (!error) {
      std::move(internal_callback).Run(value);
    }
  });
}

void WebFrameImpl::LogScriptWarning(NSString* script, NSError* error) {
  std::u16string executed_script = base::SysNSStringToUTF16(script);
  std::u16string error_string =
      base::SysNSStringToUTF16(error.userInfo[NSLocalizedDescriptionKey]);
  NSString* ns_exception = error.userInfo[@"WKJavaScriptExceptionMessage"];
  std::u16string exception = base::SysNSStringToUTF16(ns_exception);

  DLOG(WARNING) << "Script execution of:" << executed_script
                << "\nfailed with error: " << error_string
                << "\nand exception: " << exception;

  UMA_HISTOGRAM_BOOLEAN("IOS.Javascript.ScriptExecutionFailed", true);

  if (!base::FeatureList::IsEnabled(features::kLogJavaScriptErrors)) {
    return;
  }

  // Do not log invalid target frame errors. This error means that the frame is
  // no longer valid. This is an expected failure state as native code only has
  // an outdated view of the web frames (updated asyncronously via JS messages
  // or navigation callbacks).
  if (error.domain == WKErrorDomain &&
      error.code == WKErrorJavaScriptInvalidFrameTarget) {
    return;
  }

  // Some frames do not allow JavaScript execution, there is no need to report
  // this as an error as it is an expected case.
  if (error.domain == WKErrorDomain &&
      [kCannotExecuteJSInDocumentErrorMessage isEqualToString:ns_exception]) {
    return;
  }

  SCOPED_CRASH_KEY_STRING256("JavaScript", "script",
                             base::UTF16ToUTF8(executed_script));
  SCOPED_CRASH_KEY_STRING256("JavaScript", "error",
                             base::UTF16ToUTF8(error_string));
  SCOPED_CRASH_KEY_STRING256("JavaScript", "exception",
                             base::UTF16ToUTF8(exception));
  base::debug::DumpWithoutCrashing();
}

bool WebFrameImpl::ExecuteJavaScriptFunction(
    JavaScriptContentWorld* content_world,
    const std::string& name,
    const base::Value::List& parameters,
    int message_id,
    bool reply_with_result) {
  DCHECK(content_world);
  DCHECK(frame_info_);

  NSString* script = CreateFunctionCallWithParamaters(name, parameters);

  void (^completion_handler)(id, NSError*) = nil;
  if (reply_with_result) {
    base::WeakPtr<WebFrameImpl> weak_frame = weak_ptr_factory_.GetWeakPtr();
    completion_handler = ^void(id value, NSError* error) {
      if (error) {
        LogScriptWarning(script, error);
      }
      if (weak_frame) {
        weak_frame->CompleteRequest(message_id,
                                    ValueResultFromWKResult(value).get());
      }
    };
  }

  WKContentWorld* world = content_world->GetWKContentWorld();
  DCHECK(world);

  web::ExecuteJavaScript(frame_info_.webView, world, frame_info_, script,
                         completion_handler);
  return true;
}

void WebFrameImpl::CompleteRequest(int message_id, const base::Value* result) {
  auto request = pending_requests_.find(message_id);
  if (request == pending_requests_.end()) {
    return;
  }
  CompleteRequest(std::move(request->second), result);
  pending_requests_.erase(request);
}

void WebFrameImpl::CompleteRequest(
    std::unique_ptr<RequestCallbacks> request_callbacks,
    const base::Value* result) {
  request_callbacks->timeout_callback->Cancel();
  std::move(request_callbacks->completion).Run(result);
}

void WebFrameImpl::CancelRequest(int message_id) {
  CompleteRequest(message_id, /*result=*/nullptr);
}

void WebFrameImpl::CancelPendingRequests() {
  for (auto& it : pending_requests_) {
    CompleteRequest(std::move(it.second), /*result=*/nullptr);
  }
  pending_requests_.clear();
}

void WebFrameImpl::DetachFromWebState() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void WebFrameImpl::WebStateDestroyed(web::WebState* web_state) {
  CancelPendingRequests();
  DetachFromWebState();
}

WebFrameImpl::RequestCallbacks::RequestCallbacks(
    base::OnceCallback<void(const base::Value*)> completion,
    std::unique_ptr<TimeoutCallback> timeout)
    : completion(std::move(completion)), timeout_callback(std::move(timeout)) {}

WebFrameImpl::RequestCallbacks::~RequestCallbacks() {}

}  // namespace web
