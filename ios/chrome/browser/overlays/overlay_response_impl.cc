// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_response_impl.h"

// static
std::unique_ptr<OverlayResponse> OverlayResponse::Create() {
  return std::make_unique<OverlayResponseImpl>();
}

OverlayResponseImpl::OverlayResponseImpl() {}
OverlayResponseImpl::~OverlayResponseImpl() {}

base::SupportsUserData* OverlayResponseImpl::data() {
  return this;
}
