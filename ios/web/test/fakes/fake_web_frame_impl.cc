// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/test/fakes/fake_web_frame_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

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
    : frame_id_(frame_id),
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
bool FakeWebFrameImpl::CanCallJavaScriptFunction() const {
  return can_call_function_;
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
    const std::vector<base::Value>& parameters) {
  if (!can_call_function_) {
    return false;
  }

  if (call_java_script_function_callback_) {
    call_java_script_function_callback_.Run();
  }

  std::string javascript_call = std::string("__gCrWeb." + name + "(");
  bool first = true;
  for (auto& param : parameters) {
    if (!first) {
      javascript_call += ", ";
    }
    first = false;
    std::string paramString;
    base::JSONWriter::Write(param, &paramString);
    javascript_call += paramString;
  }
  javascript_call += ");";
  java_script_calls_.push_back(javascript_call);
  return can_call_function_;
}

bool FakeWebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
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
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world) {
  last_received_content_world_ = content_world;
  return CallJavaScriptFunction(name, parameters);
}

bool FakeWebFrameImpl::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  last_received_content_world_ = content_world;
  return CallJavaScriptFunction(name, parameters, std::move(callback), timeout);
}

bool FakeWebFrameImpl::ExecuteJavaScript(const std::string& script) {
  return false;
}

bool FakeWebFrameImpl::ExecuteJavaScript(
    const std::string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  return false;
}

bool FakeWebFrameImpl::ExecuteJavaScript(
    const std::string& script,
    base::OnceCallback<void(const base::Value*, bool)> callback) {
  return false;
}

void FakeWebFrameImpl::AddJsResultForFunctionCall(
    base::Value* js_result,
    const std::string& function_name) {
  result_map_[function_name] = js_result;
}

JavaScriptContentWorld* FakeWebFrameImpl::last_received_content_world() {
  return last_received_content_world_;
}

std::string FakeWebFrameImpl::GetLastJavaScriptCall() const {
  return java_script_calls_.size() == 0 ? "" : java_script_calls_.back();
}

const std::vector<std::string>& FakeWebFrameImpl::GetJavaScriptCallHistory() {
  return java_script_calls_;
}

void FakeWebFrameImpl::set_browser_state(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void FakeWebFrameImpl::set_force_timeout(bool force_timeout) {
  force_timeout_ = force_timeout;
}

void FakeWebFrameImpl::set_can_call_function(bool can_call_function) {
  can_call_function_ = can_call_function;
}

}  // namespace web
