// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frame.h"

#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "content/public/browser/render_frame_host.h"
#import "ios/web/content/web_state/content_web_state.h"

namespace web {

namespace {

void WebToContentJavaScriptCallbackAdapter(
    base::OnceCallback<void(const base::Value*)> web_callback,
    base::Value value) {
  std::move(web_callback).Run(&value);
}

void WebWithErrorToContentJavaScriptCallbackAdapter(
    ExecuteJavaScriptCallbackWithError web_callback,
    base::Value value) {
  std::move(web_callback).Run(&value, /*error=*/nil);
}

std::u16string CreateFunctionCallWithParameters(
    const std::string& name,
    const base::Value::List& parameters) {
  std::vector<std::string> parameter_strings(parameters.size());
  for (size_t i = 0; i < parameters.size(); ++i) {
    base::JSONWriter::Write(parameters[i], &parameter_strings[i]);
  }
  std::string joined_paramters = base::JoinString(parameter_strings, ",");
  return base::UTF8ToUTF16(base::StringPrintf("__gCrWeb.%s(%s)", name.c_str(),
                                              joined_paramters.c_str()));
}

}  // namespace

ContentWebFrame::ContentWebFrame(const std::string& web_frame_id,
                                 content::RenderFrameHost* render_frame_host,
                                 ContentWebState* content_web_state)
    : web_frame_id_(web_frame_id),
      content_web_state_(content_web_state),
      render_frame_host_(render_frame_host) {
  content_web_state->AddObserver(this);
  render_frame_host_->AllowInjectingJavaScript();
}

ContentWebFrame::~ContentWebFrame() {
  DetachFromWebState();
}

WebFrameInternal* ContentWebFrame::GetWebFrameInternal() {
  return this;
}

std::string ContentWebFrame::GetFrameId() const {
  return web_frame_id_;
}

bool ContentWebFrame::IsMainFrame() const {
  return render_frame_host_->IsInPrimaryMainFrame();
}

GURL ContentWebFrame::GetSecurityOrigin() const {
  // TODO(crbug.com/40260077):  Once GetSecurityOrigin is changed to return an
  // Origin instead of a URL, this should use GetLastCommittedOrigin().
  return render_frame_host_->GetLastCommittedURL().DeprecatedGetOriginAsURL();
}

BrowserState* ContentWebFrame::GetBrowserState() {
  return content_web_state_->GetBrowserState();
  ;
}

base::WeakPtr<WebFrame> ContentWebFrame::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ContentWebFrame::CallJavaScriptFunction(
    const std::string& name,
    const base::Value::List& parameters) {
  return ExecuteJavaScript(CreateFunctionCallWithParameters(name, parameters));
}

bool ContentWebFrame::CallJavaScriptFunction(
    const std::string& name,
    const base::Value::List& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  // TODO(crbug.com/40260088): Handle timeouts.
  return ExecuteJavaScript(CreateFunctionCallWithParameters(name, parameters),
                           std::move(callback));
}

bool ContentWebFrame::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world) {
  // TODO(crbug.com/40260088): Handle injecting into an isolated world.
  return ExecuteJavaScript(CreateFunctionCallWithParameters(name, parameters));
}

bool ContentWebFrame::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const base::Value::List& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  // TODO(crbug.com/40260088): Handle timeouts and injecting into an isolated
  // world.
  return ExecuteJavaScript(CreateFunctionCallWithParameters(name, parameters),
                           std::move(callback));
}

bool ContentWebFrame::ExecuteJavaScriptInContentWorld(
    const std::u16string& script,
    JavaScriptContentWorld* content_world,
    ExecuteJavaScriptCallbackWithError callback) {
  render_frame_host_->ExecuteJavaScript(
      script, base::BindOnce(&WebWithErrorToContentJavaScriptCallbackAdapter,
                             std::move(callback)));
  return true;
}

bool ContentWebFrame::ExecuteJavaScript(const std::u16string& script) {
  render_frame_host_->ExecuteJavaScript(
      script, content::RenderFrameHost::JavaScriptResultCallback());
  return true;
}

bool ContentWebFrame::ExecuteJavaScript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  render_frame_host_->ExecuteJavaScript(
      script, base::BindOnce(&WebToContentJavaScriptCallbackAdapter,
                             std::move(callback)));
  return true;
}

bool ContentWebFrame::ExecuteJavaScript(
    const std::u16string& script,
    ExecuteJavaScriptCallbackWithError callback) {
  render_frame_host_->ExecuteJavaScript(
      script, base::BindOnce(&WebWithErrorToContentJavaScriptCallbackAdapter,
                             std::move(callback)));
  return true;
}

void ContentWebFrame::DetachFromWebState() {
  if (content_web_state_) {
    content_web_state_->RemoveObserver(this);
    content_web_state_ = nullptr;
  }
}

void ContentWebFrame::WebStateDestroyed(web::WebState* web_state) {
  DetachFromWebState();
}

}  // namespace web
