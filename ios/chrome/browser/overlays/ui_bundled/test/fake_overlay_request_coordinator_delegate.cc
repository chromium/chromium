// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"

#include "base/containers/contains.h"

FakeOverlayRequestCoordinatorDelegate::FakeOverlayRequestCoordinatorDelegate() =
    default;
FakeOverlayRequestCoordinatorDelegate::
    ~FakeOverlayRequestCoordinatorDelegate() = default;

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenPresented(
    OverlayRequest* request) const {
  return base::Contains(states_, request) &&
         states_.at(request) == PresentationState::kPresented;
}

bool FakeOverlayRequestCoordinatorDelegate::HasUIBeenDismissed(
    OverlayRequest* request) const {
  return base::Contains(states_, request) &&
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
