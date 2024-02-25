// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

FakeOverlayPresentationContext::FakeOverlayPresentationContext() = default;
FakeOverlayPresentationContext::~FakeOverlayPresentationContext() = default;

FakeOverlayPresentationContext::PresentationState
FakeOverlayPresentationContext::GetPresentationState(OverlayRequest* request) {
  return states_[request].presentation_state;
}

void FakeOverlayPresentationContext::SetDismissalCallbacksEnabled(
    bool enabled) {
  if (dismissal_callbacks_enabled_ == enabled)
    return;
  dismissal_callbacks_enabled_ = enabled;
  if (dismissal_callbacks_enabled_)
    RunPresentedRequestDismissalCallback();
}

bool FakeOverlayPresentationContext::AreDismissalCallbacksEnabled() const {
  return dismissal_callbacks_enabled_;
}

void FakeOverlayPresentationContext::SimulateDismissalForRequest(
    OverlayRequest* request,
    OverlayDismissalReason reason) {
  DCHECK_EQ(presented_request_, request);
  FakeUIState& state = states_[request];
  state.dismissal_reason = reason;
  RunPresentedRequestDismissalCallback();
}

void FakeOverlayPresentationContext::SetPresentationCapabilities(
    UIPresentationCapabilities capabilities) {
  if (capabilities_ == capabilities)
    return;

  for (auto& observer : observers_) {
    observer.OverlayPresentationContextWillChangePresentationCapabilities(
        this, capabilities);
  }
  capabilities_ = capabilities;
  for (auto& observer : observers_) {
    observer.OverlayPresentationContextDidChangePresentationCapabilities(this);
  }
}

void FakeOverlayPresentationContext::AddObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeOverlayPresentationContext::RemoveObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.RemoveObserver(observer);
}

OverlayPresentationContext::UIPresentationCapabilities
FakeOverlayPresentationContext::GetPresentationCapabilities() const {
  return capabilities_;
}

bool FakeOverlayPresentationContext::CanShowUIForRequest(
    OverlayRequest* request,
    UIPresentationCapabilities capabilities) const {
  return capabilities !=
         OverlayPresentationContext::UIPresentationCapabilities::kNone;
}

bool FakeOverlayPresentationContext::CanShowUIForRequest(
    OverlayRequest* request) const {
  return CanShowUIForRequest(request, capabilities_) && !IsShowingOverlayUI();
}

bool FakeOverlayPresentationContext::IsShowingOverlayUI() const {
  return presented_request_;
}

void FakeOverlayPresentationContext::PrepareToShowOverlayUI(
    OverlayRequest* request) {}

void FakeOverlayPresentationContext::ShowOverlayUI(
    OverlayRequest* request,
    OverlayPresentationCallback presentation_callback,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK(!presented_request_);
  presented_request_ = request;
  FakeUIState& state = states_[request];
  state.presentation_state = PresentationState::kPresented;
  state.dismissal_callback = std::move(dismissal_callback);
  std::move(presentation_callback).Run();
}

void FakeOverlayPresentationContext::HideOverlayUI(OverlayRequest* request) {
  SimulateDismissalForRequest(request, OverlayDismissalReason::kHiding);
}

void FakeOverlayPresentationContext::CancelOverlayUI(OverlayRequest* request) {
  FakeUIState& state = states_[request];
  if (state.presentation_state == PresentationState::kPresented) {
    SimulateDismissalForRequest(request, OverlayDismissalReason::kCancellation);
  } else {
    state.presentation_state = PresentationState::kCancelled;
  }
}

void FakeOverlayPresentationContext::SetUIDisabled(bool disabled) {}

bool FakeOverlayPresentationContext::IsUIDisabled() {
  return false;
}

void FakeOverlayPresentationContext::RunPresentedRequestDismissalCallback() {
  if (!dismissal_callbacks_enabled_ || !presented_request_)
    return;
  FakeUIState& state = states_[presented_request_];
  OverlayDismissalReason reason = state.dismissal_reason;
  switch (reason) {
    case OverlayDismissalReason::kUserInteraction:
      state.presentation_state = PresentationState::kUserDismissed;
      break;
    case OverlayDismissalReason::kHiding:
      state.presentation_state = PresentationState::kHidden;
      break;
    case OverlayDismissalReason::kCancellation:
      state.presentation_state = PresentationState::kCancelled;
      break;
  }
  presented_request_ = nullptr;
  std::move(state.dismissal_callback).Run(reason);
}

FakeOverlayPresentationContext::FakeUIState::FakeUIState() = default;

FakeOverlayPresentationContext::FakeUIState::~FakeUIState() = default;
