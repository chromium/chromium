// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/overlay_request_impl.h"

#include "base/no_destructor.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response.h"

namespace {

// Generates a new OverlayRequestId.
OverlayRequestId GenerateNextOverlayRequestId() {
  static OverlayRequestId::Generator kGenerator;
  return kGenerator.GenerateNextId();
}

}  // namespace

// static
std::unique_ptr<OverlayRequest> OverlayRequest::Create() {
  return std::make_unique<OverlayRequestImpl>();
}

OverlayRequestImpl::OverlayRequestImpl()
    : request_id_(GenerateNextOverlayRequestId()) {}

OverlayRequestImpl::~OverlayRequestImpl() {
  callback_manager_.ExecuteCompletionCallbacks();
}

OverlayCallbackManager* OverlayRequestImpl::GetCallbackManager() {
  return &callback_manager_;
}

web::WebState* OverlayRequestImpl::GetQueueWebState() {
  return queue_web_state_;
}

OverlayRequestId OverlayRequestImpl::GetRequestId() const {
  return request_id_;
}

base::SupportsUserData* OverlayRequestImpl::data() {
  return this;
}
