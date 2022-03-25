// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frame_impl.h"

#import <Foundation/Foundation.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#import "ios/web/js_messaging/java_script_content_world.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kJavaScriptReplyCommandPrefix[] = "frameMessaging_";

// Creates a JavaScript string for executing the function __gCrWeb.|name| with
// |parameters|.
NSString* CreateFunctionCallWithParamaters(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
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
}

namespace web {

const double kJavaScriptFunctionCallDefaultTimeout = 100.0;

WebFrameImpl::WebFrameImpl(WKFrameInfo* frame_info,
                           const std::string& frame_id,
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_info_(frame_info),
      frame_id_(frame_id),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state),
      weak_ptr_factory_(this) {
  DCHECK(frame_info_);
  DCHECK(web_state_);
  web_state->AddObserver(this);

  subscription_ = web_state->AddScriptCommandCallback(
      base::BindRepeating(&WebFrameImpl::OnJavaScriptReply,
                          base::Unretained(this), base::Unretained(web_state)),
      GetScriptCommandPrefix());
}

WebFrameImpl::~WebFrameImpl() {
  CancelPendingRequests();
  DetachFromWebState();
}

WebFrameInternal* WebFrameImpl::GetWebFrameInternal() {
  return this;
}

void WebFrameImpl::SetEncryptionKey(
    std::unique_ptr<crypto::SymmetricKey> frame_key) {
  frame_key_ = std::move(frame_key);
}

void WebFrameImpl::SetNextMessageId(int message_id) {
  next_message_id_ = message_id;
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

bool WebFrameImpl::CanCallJavaScriptFunction() const {
  // JavaScript can always be called on the main frame without encryption
  // because calling the function directly on the webstate with
  // |ExecuteJavaScript| is secure. However, iframes require an encryption key
  // in order to securely pass the function name and parameters to the frame.
  return is_main_frame_ || frame_key_;
}

BrowserState* WebFrameImpl::GetBrowserState() {
  return GetWebState()->GetBrowserState();
}

const std::string WebFrameImpl::EncryptPayload(
    base::Value payload,
    const std::string& additiona_data) {
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&frame_key_->key());

  std::string payload_json;
  base::JSONWriter::Write(payload, &payload_json);
  std::string payload_iv;
  crypto::RandBytes(base::WriteInto(&payload_iv, aead.NonceLength() + 1),
                    aead.NonceLength());
  std::string payload_ciphertext;
  if (!aead.Seal(payload_json, payload_iv, additiona_data,
                 &payload_ciphertext)) {
    LOG(ERROR) << "Error sealing message payload for WebFrame.";
    return std::string();
  }
  std::string encoded_payload_iv;
  base::Base64Encode(payload_iv, &encoded_payload_iv);
  std::string encoded_payload;
  base::Base64Encode(payload_ciphertext, &encoded_payload);

  std::string payload_string;
  base::Value payload_dict(base::Value::Type::DICTIONARY);
  payload_dict.SetKey("payload", base::Value(encoded_payload));
  payload_dict.SetKey("iv", base::Value(encoded_payload_iv));
  base::JSONWriter::Write(payload_dict, &payload_string);
  return payload_string;
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world,
    bool reply_with_result) {
  int message_id = next_message_id_;
  next_message_id_++;

  if (content_world && content_world->GetWKContentWorld()) {
    return ExecuteJavaScriptFunction(content_world, name, parameters,
                                     message_id, reply_with_result);
  }

  if (!CanCallJavaScriptFunction()) {
    return false;
  }

  if (!frame_key_) {
    return ExecuteJavaScriptFunction(name, parameters, message_id,
                                     reply_with_result);
  }

  base::Value message_payload(base::Value::Type::DICTIONARY);
  message_payload.SetKey("messageId", base::Value(message_id));
  message_payload.SetKey("replyWithResult", base::Value(reply_with_result));
  const std::string& encrypted_message_json =
      EncryptPayload(std::move(message_payload), std::string());

  base::Value function_payload(base::Value::Type::DICTIONARY);
  function_payload.SetKey("functionName", base::Value(name));
  base::ListValue parameters_value(parameters);
  function_payload.SetKey("parameters", std::move(parameters_value));
  const std::string& encrypted_function_json = EncryptPayload(
      std::move(function_payload), base::NumberToString(message_id));

  if (encrypted_message_json.empty() || encrypted_function_json.empty()) {
    // Sealing the payload failed.
    return false;
  }

  std::string script =
      base::StringPrintf("__gCrWeb.message.routeMessage(%s, %s, '%s')",
                         encrypted_message_json.c_str(),
                         encrypted_function_json.c_str(), frame_id_.c_str());
  GetWebState()->ExecuteJavaScript(base::UTF8ToUTF16(script));

  return true;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  return CallJavaScriptFunctionInContentWorld(name, parameters,
                                              /*content_world=*/nullptr,
                                              /*reply_with_result=*/false);
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world) {
  return CallJavaScriptFunctionInContentWorld(name, parameters, content_world,
                                              /*reply_with_result=*/false);
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  return CallJavaScriptFunctionInContentWorld(name, parameters,
                                              /*content_world=*/nullptr,
                                              std::move(callback), timeout);
}

bool WebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
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

bool WebFrameImpl::ExecuteJavaScript(const std::string& script) {
  return ExecuteJavaScript(script,
                           base::DoNothingAs<void(const base::Value*)>());
}

bool WebFrameImpl::ExecuteJavaScript(
    const std::string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  ExecuteJavaScriptCallbackWithError callback_with_error =
      ExecuteJavaScriptCallbackAdapter(std::move(callback));

  return ExecuteJavaScript(script, std::move(callback_with_error));
}

bool WebFrameImpl::ExecuteJavaScript(
    const std::string& script,
    ExecuteJavaScriptCallbackWithError callback) {
  DCHECK(frame_info_);

  if (!IsMainFrame()) {
    return false;
  }

  NSString* ns_script = base::SysUTF8ToNSString(script);
  __block auto internal_callback = std::move(callback);
  void (^completion_handler)(id, NSError*) = ^void(id value, NSError* error) {
    if (error) {
      LogScriptWarning(ns_script, error);
      std::move(internal_callback).Run(nullptr, true);
    } else {
      std::move(internal_callback)
          .Run(ValueResultFromWKResult(value).get(), false);
    }
  };

  web::ExecuteJavaScript(frame_info_.webView, WKContentWorld.pageWorld,
                         frame_info_, ns_script, completion_handler);
  return true;
}

WebFrame::ExecuteJavaScriptCallbackWithError
WebFrameImpl::ExecuteJavaScriptCallbackAdapter(
    base::OnceCallback<void(const base::Value*)> callback) {
  // Because blocks treat scoped-variables
  // as const, we have to redefine the callback with the
  // __block keyword to be able to run the callback inside
  // the completion handler.
  __block auto internal_callback = std::move(callback);
  return base::BindOnce(^(const base::Value* value, bool error) {
    if (!error) {
      std::move(internal_callback).Run(value);
    }
  });
}

void WebFrameImpl::LogScriptWarning(NSString* script, NSError* error) {
  DLOG(WARNING) << "Script execution of:" << base::SysNSStringToUTF16(script)
                << "\nfailed with error: "
                << base::SysNSStringToUTF16(
                       error.userInfo[NSLocalizedDescriptionKey]);
}

bool WebFrameImpl::ExecuteJavaScriptFunction(
    JavaScriptContentWorld* content_world,
    const std::string& name,
    const std::vector<base::Value>& parameters,
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
        DLOG(WARNING) << "Script execution of:"
                      << base::SysNSStringToUTF16(script)
                      << "\nfailed with error: "
                      << base::SysNSStringToUTF16(
                             error.userInfo[NSLocalizedDescriptionKey]);
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

bool WebFrameImpl::ExecuteJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    int message_id,
    bool reply_with_result) {
  if (!IsMainFrame()) {
    return false;
  }

  NSString* script = CreateFunctionCallWithParamaters(name, parameters);
  if (!reply_with_result) {
    GetWebState()->ExecuteJavaScript(base::SysNSStringToUTF16(script));
    return true;
  }

  base::WeakPtr<WebFrameImpl> weak_frame = weak_ptr_factory_.GetWeakPtr();
  GetWebState()->ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                   base::BindOnce(^(const base::Value* result) {
                                     if (weak_frame) {
                                       weak_frame->CompleteRequest(message_id,
                                                                   result);
                                     }
                                   }));
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

void WebFrameImpl::OnJavaScriptReply(web::WebState* web_state,
                                     const base::Value& command_json,
                                     const GURL& page_url,
                                     bool interacting,
                                     WebFrame* sender_frame) {
  const std::string* command_string = command_json.FindStringKey("command");
  if (!command_string ||
      *command_string != (GetScriptCommandPrefix() + ".reply")) {
    return;
  }

  absl::optional<double> message_id = command_json.FindDoubleKey("messageId");
  if (!message_id) {
    return;
  }

  CompleteRequest(static_cast<int>(*message_id),
                  command_json.FindKey("result"));
}

void WebFrameImpl::DetachFromWebState() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

const std::string WebFrameImpl::GetScriptCommandPrefix() {
  return kJavaScriptReplyCommandPrefix + frame_id_;
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
