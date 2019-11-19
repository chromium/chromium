// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"

FakeOverlayRequestCoordinatorDelegate::FakeOverlayRequestCoordinatorDelegate() =
    default;
FakeOverlayRequestCoordinatorDelegate::
    ~FakeOverlayRequestCoordinatorDelegate() = default;

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenPresented(
    OverlayRequest* request) const {
  return states_.find(request) != states_.end() &&
         states_.at(request) == PresentationState::kPresented;
}

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenDismissed(
    OverlayRequest* request) const {
  return states_.find(request) != states_.end() &&
         states_.at(request) == PresentationState::kDismissed;
}

void FakeOverlayRequestCoordinatorDelegate::OverlayUIDidFinishPresentation(
    OverlayRequest* request) {
  states_[request] = PresentationState::kPresented;
}

void FakeOverlayRequestCoordinatorDelegate::OverlayUIDidFinishDismissal(
    OverlayRequest* request) {
  states_[request] = PresentationState::kDismissed;
}
