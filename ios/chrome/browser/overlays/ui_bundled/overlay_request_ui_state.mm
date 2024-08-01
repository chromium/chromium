// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_ui_state.h"

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator.h"

OverlayRequestUIState::OverlayRequestUIState(OverlayRequest* request)
    : request_(request) {
  DCHECK(request_);
}

OverlayRequestUIState::~OverlayRequestUIState() {
  if (has_callback()) {
    set_dismissal_reason(OverlayDismissalReason::kCancellation);
    OverlayUIWasDismissed();
  }
}

void OverlayRequestUIState::OverlayPresentionRequested(
    OverlayPresentationCallback presentation_callback,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK(presentation_callback_.is_null());
  DCHECK(dismissal_callback_.is_null());
  DCHECK(!presentation_callback.is_null());
  DCHECK(!dismissal_callback.is_null());
  presentation_callback_ = std::move(presentation_callback);
  dismissal_callback_ = std::move(dismissal_callback);
  // The default dismissal reason is kUserInteraction.  This is to avoid
  // additional bookkeeping for overlays dismissed by user interaction.
  // Overlays explicitly dismissed by OverlayPresenter set the reason to kHide
  // or kCancellation before dismissal.
  dismissal_reason_ = OverlayDismissalReason::kUserInteraction;
}

void OverlayRequestUIState::OverlayUIWillBePresented(
    OverlayRequestCoordinator* coordinator) {
  DCHECK(coordinator);
  coordinator_ = coordinator;
}

void OverlayRequestUIState::OverlayUIWasPresented() {
  has_ui_been_presented_ = true;
  std::move(presentation_callback_).Run();
}

void OverlayRequestUIState::OverlayUIWasDismissed() {
  DCHECK(has_callback());
  std::move(dismissal_callback_).Run(dismissal_reason_);
}
