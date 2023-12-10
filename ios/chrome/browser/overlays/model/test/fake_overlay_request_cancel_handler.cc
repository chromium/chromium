// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/test/fake_overlay_request_cancel_handler.h"

FakeOverlayRequestCancelHandler::FakeOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue)
    : OverlayRequestCancelHandler(request, queue) {}

FakeOverlayRequestCancelHandler::~FakeOverlayRequestCancelHandler() = default;

void FakeOverlayRequestCancelHandler::TriggerCancellation() {
  CancelRequest();
}
