// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_request_cancel_handler.h"

FakeOverlayRequestCancelHandler::FakeOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue)
    : OverlayRequestCancelHandler(request, queue) {}

FakeOverlayRequestCancelHandler::~FakeOverlayRequestCancelHandler() = default;

void FakeOverlayRequestCancelHandler::TriggerCancellation() {
  CancelRequest();
}
