// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_web_frame_impl.h"

#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace web {

// Frame ids are base16 string of 128 bit numbers.
const char kMainFakeFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd1";
const char kInvalidFrameId[] = "1effd8f52a067c8d3a01762d3c4;dfd1";
const char kChildFakeFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd2";
const char kChildFakeFrameId2[] = "1effd8f52a067c8d3a01762d3c41dfd3";

// static
std::unique_ptr<FakeWebFrame> FakeWebFrame::Create(const std::string& frame_id,
                                                   bool is_main_frame,
                                                   GURL security_origin) {
  return std::make_unique<FakeWebFrameImpl>(frame_id, is_main_frame,
                                            security_origin);
}

// static
std::unique_ptr<FakeWebFrame> FakeWebFrame::CreateMainWebFrame(
    GURL security_origin) {
  return std::make_unique<FakeWebFrameImpl>(
      kMainFakeFrameId, /*is_main_frame=*/true, security_origin);
}

// static
std::unique_ptr<FakeWebFrame> FakeWebFrame::CreateChildWebFrame(
    GURL security_origin) {
  return std::make_unique<FakeWebFrameImpl>(
      kChildFakeFrameId, /*is_main_frame=*/false, security_origin);
}

FakeWebFrameImpl::FakeWebFrameImpl(const std::string& frame_id,
                                   bool is_main_frame,
                                   GURL security_origin)
    : frame_id_(base::ToLowerASCII(frame_id)),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin) {}

FakeWebFrameImpl::~FakeWebFrameImpl() {}

WebFrameInternal* FakeWebFrameImpl::GetWebFrameInternal() {
  return this;
}

std::string FakeWebFrameImpl::GetFrameId() const {
  return frame_id_;
}
bool FakeWebFrameImpl::IsMainFrame() const {
  return is_main_frame_;
}
GURL FakeWebFrameImpl::GetSecurityOrigin() const {
  return security_origin_;
}

BrowserState* FakeWebFrameImpl::GetBrowserState() {
  return browser_state_;
}

void FakeWebFrameImpl::set_call_java_script_function_callback(
    base::RepeatingClosure callback) {
  call_java_script_function_callback_ = std::move(callback);
}

bool FakeWebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const base::Value::List& parameters) {
  if (call_java_script_function_callback_) {
    call_java_script_function_callback_.Run();
  }

  std::u16string javascript_call =
      std::u16string(u"__gCrWeb." + base::UTF8ToUTF16(name) + u"(");
  bool first = true;
  for (auto& param : parameters) {
    if (!first) {
      javascript_call += u", ";
    }
    first = false;
    std::string paramString;
    base::JSONWriter::Write(param, &paramString);
    javascript_call += base::UTF8ToUTF16(paramString);
  }
  javascript_call += u");";
  java_script_calls_.push_back(javascript_call);
  return true;
}

bool FakeWebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const base::Value::List& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  bool success = CallJavaScriptFunction(name, parameters);
  if (!success) {
    return false;
  }

  if (force_timeout_) {
    web::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr), timeout);
  } else {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result_map_[name]));
  }
  return true;
}

bool FakeWebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world) {
  return CallJavaScriptFunction(name, parameters);
}

bool FakeWebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  return CallJavaScriptFunction(name, parameters, std::move(callback), timeout);
}

bool FakeWebFrameImpl::ExecuteJavaScript(const std::u16string& script) {
  java_script_calls_.push_back(script);
  return false;
}

bool FakeWebFrameImpl::ExecuteJavaScript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  java_script_calls_.push_back(script);

  const base::Value* result = executed_js_result_map_[script];

  if (!callback.is_null()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  return result != nullptr;
}

bool FakeWebFrameImpl::ExecuteJavaScript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*, NSError*)> callback) {
  java_script_calls_.push_back(script);

  const base::Value* result = executed_js_result_map_[script];
  NSError* error = nil;
  if (!result) {
    error = [[NSError alloc] initWithDomain:@"" code:0 userInfo:nil];
  }

  if (!callback.is_null()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result, error));
  }

  return !error;
}

base::WeakPtr<WebFrame> FakeWebFrameImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FakeWebFrameImpl::ExecuteJavaScriptInContentWorld(
    const std::u16string& script,
    JavaScriptContentWorld* content_world,
    ExecuteJavaScriptCallbackWithError callback) {
  return ExecuteJavaScript(script, std::move(callback));
}

void FakeWebFrameImpl::AddJsResultForFunctionCall(
    base::Value* js_result,
    const std::string& function_name) {
  result_map_[function_name] = js_result;
}

void FakeWebFrameImpl::AddResultForExecutedJs(
    base::Value* js_result,
    const std::u16string& executed_js) {
  executed_js_result_map_[executed_js] = js_result;
}

std::u16string FakeWebFrameImpl::GetLastJavaScriptCall() const {
  return java_script_calls_.size() == 0 ? u"" : java_script_calls_.back();
}

const std::vector<std::u16string>&
FakeWebFrameImpl::GetJavaScriptCallHistory() {
  return java_script_calls_;
}

void FakeWebFrameImpl::ClearJavaScriptCallHistory() {
  java_script_calls_.clear();
}

void FakeWebFrameImpl::set_browser_state(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void FakeWebFrameImpl::set_force_timeout(bool force_timeout) {
  force_timeout_ = force_timeout;
}

}  // namespace web
