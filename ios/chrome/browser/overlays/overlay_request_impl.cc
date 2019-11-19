// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_request_impl.h"

#include "ios/chrome/browser/overlays/public/overlay_response.h"

// static
std::unique_ptr<OverlayRequest> OverlayRequest::Create() {
  return std::make_unique<OverlayRequestImpl>();
}

OverlayRequestImpl::OverlayRequestImpl() {}

OverlayRequestImpl::~OverlayRequestImpl() {
  if (!callback_.is_null())
    std::move(callback_).Run(response());
}

void OverlayRequestImpl::set_response(
    std::unique_ptr<OverlayResponse> response) {
  response_ = std::move(response);
}

OverlayResponse* OverlayRequestImpl::response() const {
  return response_.get();
}

void OverlayRequestImpl::set_callback(OverlayCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
}

base::SupportsUserData* OverlayRequestImpl::data() {
  return this;
}
