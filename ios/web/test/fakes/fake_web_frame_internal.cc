// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/test/fakes/fake_web_frame_internal.h"

namespace web {

std::string FakeWebFrameInternal::GetFrameId() const {
  return FakeWebFrame::GetFrameId();
}

bool FakeWebFrameInternal::IsMainFrame() const {
  return FakeWebFrame::IsMainFrame();
}

GURL FakeWebFrameInternal::GetSecurityOrigin() const {
  return FakeWebFrame::GetSecurityOrigin();
}

bool FakeWebFrameInternal::CanCallJavaScriptFunction() const {
  return FakeWebFrame::CanCallJavaScriptFunction();
}

BrowserState* FakeWebFrameInternal::GetBrowserState() {
  return FakeWebFrame::GetBrowserState();
}

bool FakeWebFrameInternal::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  return FakeWebFrame::CallJavaScriptFunction(name, parameters);
}

bool FakeWebFrameInternal::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  return FakeWebFrame::CallJavaScriptFunction(name, parameters,
                                              std::move(callback), timeout);
}

bool FakeWebFrameInternal::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world) {
  last_received_content_world_ = content_world;
  return FakeWebFrame::CallJavaScriptFunction(name, parameters);
}

bool FakeWebFrameInternal::CallJavaScriptFunctionInContentWorld(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    JavaScriptContentWorld* content_world,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  last_received_content_world_ = content_world;
  return FakeWebFrame::CallJavaScriptFunction(name, parameters,
                                              std::move(callback), timeout);
}

JavaScriptContentWorld* FakeWebFrameInternal::last_received_content_world() {
  return last_received_content_world_;
}

FakeWebFrameInternal::FakeWebFrameInternal(const std::string& frame_id,
                                           bool is_main_frame,
                                           GURL security_origin)
    : FakeWebFrame(frame_id, is_main_frame, security_origin) {}

FakeWebFrameInternal::~FakeWebFrameInternal() = default;

// FakeMainWebFrameInternal
FakeMainWebFrameInternal::FakeMainWebFrameInternal(GURL security_origin)
    : FakeWebFrameInternal(kMainFakeFrameId,
                           /*is_main_frame=*/true,
                           security_origin) {}

FakeMainWebFrameInternal::~FakeMainWebFrameInternal() {}

// FakeChildWebFrameInternal
FakeChildWebFrameInternal::FakeChildWebFrameInternal(GURL security_origin)
    : FakeWebFrameInternal(kChildFakeFrameId,
                           /*is_main_frame=*/false,
                           security_origin) {}

FakeChildWebFrameInternal::~FakeChildWebFrameInternal() {}

}  // namespace web
