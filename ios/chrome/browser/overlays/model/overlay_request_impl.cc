// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/overlay_request_impl.h"

#include "ios/chrome/browser/overlays/model/public/overlay_response.h"

// static
std::unique_ptr<OverlayRequest> OverlayRequest::Create() {
  return std::make_unique<OverlayRequestImpl>();
}

OverlayRequestImpl::OverlayRequestImpl() {}

OverlayRequestImpl::~OverlayRequestImpl() {
  callback_manager_.ExecuteCompletionCallbacks();
}

OverlayCallbackManager* OverlayRequestImpl::GetCallbackManager() {
  return &callback_manager_;
}

web::WebState* OverlayRequestImpl::GetQueueWebState() {
  return queue_web_state_;
}

base::SupportsUserData* OverlayRequestImpl::data() {
  return this;
}
