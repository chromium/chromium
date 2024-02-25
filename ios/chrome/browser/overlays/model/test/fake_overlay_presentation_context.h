// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_

#include <map>

#import "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request.h"

// Fake implementation of OverlayUIDelegate used for testing.
class FakeOverlayPresentationContext : public OverlayPresentationContext {
 public:
  FakeOverlayPresentationContext();
  ~FakeOverlayPresentationContext() override;

  // Enum describing the state of the overlay UI.
  enum class PresentationState {
    // Default state.  No overlays have been presented.
    kNotPresented,
    // An overlay is currently being presented.
    kPresented,
    // A presented overlay was dismissed by user interaction.
    kUserDismissed,
    // A presented overlay was hidden.
    kHidden,
    // A presented overlay was cancelled.
    kCancelled,
  };
  // Returns the presentation state for the overlay UI.
  PresentationState GetPresentationState(OverlayRequest* request);

  // Whether dismissal calbacks are enabled.  If set to false, faked dismissals
  // in this context will not trigger the request's dismissal callback.  If the
  // presented request has been dismissed while callbacks are disabled, the
  // presented request's dismissal callback will be executed upon being
  // re-enabled.
  void SetDismissalCallbacksEnabled(bool enabled);
  bool AreDismissalCallbacksEnabled() const;

  // Simulates the dismissal of overlay UI for `reason`.  If dismissal callbacks
  // are enabled, triggers execution of `request`'s OverlayDismissalCallback.
  void SimulateDismissalForRequest(OverlayRequest* request,
                                   OverlayDismissalReason reason);

  // Sets the presentation capabilities for the fake context.  Presentation does
  // not actually occur with FakeOverlayPresentationContext, so the capabilities
  // are set directly rather than updating them based on provided base
  // UIViewControllers as is done in OverlayPresentationContextImpl.
  void SetPresentationCapabilities(UIPresentationCapabilities capabilities);

  // OverlayUIDelegate:
  void AddObserver(OverlayPresentationContextObserver* observer) override;
  void RemoveObserver(OverlayPresentationContextObserver* observer) override;
  UIPresentationCapabilities GetPresentationCapabilities() const override;
  bool CanShowUIForRequest(
      OverlayRequest* request,
      UIPresentationCapabilities capabilities) const override;
  bool CanShowUIForRequest(OverlayRequest* request) const override;
  bool IsShowingOverlayUI() const override;
  void PrepareToShowOverlayUI(OverlayRequest* request) override;
  void ShowOverlayUI(OverlayRequest* request,
                     OverlayPresentationCallback presentation_callback,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayRequest* request) override;
  void CancelOverlayUI(OverlayRequest* request) override;
  void SetUIDisabled(bool disabled) override;
  bool IsUIDisabled() override;

 private:
  // Struct used to store state for the fake presentation context.
  struct FakeUIState {
    FakeUIState();
    ~FakeUIState();

    PresentationState presentation_state = PresentationState::kNotPresented;
    OverlayDismissalReason dismissal_reason =
        OverlayDismissalReason::kUserInteraction;
    OverlayDismissalCallback dismissal_callback;
  };

  // Runs the dismissal callback for the presented request with the most recent
  // dismissal reason stored in its FakeUIState.
  void RunPresentedRequestDismissalCallback();

  // Whether dismissal callback execution is enabled.
  bool dismissal_callbacks_enabled_ = true;
  // The presented request.  Null if no request is presented.
  raw_ptr<OverlayRequest> presented_request_ = nullptr;
  // The UI states for each request.
  std::map<OverlayRequest*, FakeUIState> states_;
  // The capabilities of the context.
  UIPresentationCapabilities capabilities_ =
      static_cast<UIPresentationCapabilities>(
          UIPresentationCapabilities::kContained |
          UIPresentationCapabilities::kPresented);
  base::ObserverList<OverlayPresentationContextObserver,
                     /* check_empty= */ true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
