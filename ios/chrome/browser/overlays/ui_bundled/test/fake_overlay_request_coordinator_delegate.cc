// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"


FakeOverlayRequestCoordinatorDelegate::FakeOverlayRequestCoordinatorDelegate() =
    default;
FakeOverlayRequestCoordinatorDelegate::
    ~FakeOverlayRequestCoordinatorDelegate() = default;

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenPresented(
    OverlayRequest* request) const {
  if (!request) {
    return false;
  }
  OverlayRequestId request_id = request->GetRequestId();
  return states_.contains(request_id) &&
         states_.at(request_id) == PresentationState::kPresented;
}

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenDismissed(
    OverlayRequest* request) const {
  if (!request) {
    return false;
  }
  OverlayRequestId request_id = request->GetRequestId();
  return states_.contains(request_id) &&
         states_.at(request_id) == PresentationState::kDismissed;
}

void FakeOverlayRequestCoordinatorDelegate::OverlayUIDidFinishPresentation(
    OverlayRequest* request) {
  if (request) {
    states_[request->GetRequestId()] = PresentationState::kPresented;
  }
}

void FakeOverlayRequestCoordinatorDelegate::OverlayUIDidFinishDismissal(
    OverlayRequestId request_id) {
  states_[request_id] = PresentationState::kDismissed;
}
