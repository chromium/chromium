// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frame.h"

#import "content/public/browser/render_frame_host.h"
#import "ios/web/content/web_state/content_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWebFrame::ContentWebFrame(const std::string& web_frame_id,
                                 content::RenderFrameHost* render_frame_host,
                                 ContentWebState* content_web_state)
    : web_frame_id_(web_frame_id),
      content_web_state_(content_web_state),
      render_frame_host_(render_frame_host) {
  content_web_state->AddObserver(this);
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
  // TODO(crbug.com/1423501):  Once GetSecurityOrigin is changed to return an
  // Origin instead of a URL, this should use GetLastCommittedOrigin().
  return render_frame_host_->GetLastCommittedURL();
}

BrowserState* ContentWebFrame::GetBrowserState() {
  return content_web_state_->GetBrowserState();
  ;
}

bool ContentWebFrame::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::ExecuteJavaScript(const std::u16string& script) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::ExecuteJavaScript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
}

bool ContentWebFrame::ExecuteJavaScript(
    const std::u16string& script,
    ExecuteJavaScriptCallbackWithError callback) {
  // TODO(crbug.com/1423527): Implement this.
  return false;
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
