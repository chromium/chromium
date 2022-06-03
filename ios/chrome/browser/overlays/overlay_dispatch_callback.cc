// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"

#include "base/check.h"
#include "ios/chrome/browser/overlays/public/overlay_response_support.h"

OverlayDispatchCallback::OverlayDispatchCallback(
    base::RepeatingCallback<void(OverlayResponse* response)> callback,
    const OverlayResponseSupport* support)
    : callback_(std::move(callback)), response_support_(support) {
  DCHECK(!callback_.is_null());
  DCHECK(response_support_);
}

OverlayDispatchCallback::OverlayDispatchCallback(
    OverlayDispatchCallback&& other)
    : callback_(std::move(other.callback_)),
      response_support_(other.response_support_) {}

OverlayDispatchCallback::~OverlayDispatchCallback() = default;

void OverlayDispatchCallback::Run(OverlayResponse* response) {
  if (response_support_->IsResponseSupported(response))
    callback_.Run(response);
}
