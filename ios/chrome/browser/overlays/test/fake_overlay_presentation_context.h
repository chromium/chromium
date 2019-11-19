// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_

#include <map>

#include "base/observer_list.h"
#include "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

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

  // Simulates the dismissal of overlay UI for |reason|.
  void SimulateDismissalForRequest(OverlayRequest* request,
                                   OverlayDismissalReason reason);

  void SetPresentationCapabilities(UIPresentationCapabilities capabilities);

  // OverlayUIDelegate:
  void AddObserver(OverlayPresentationContextObserver* observer) override;
  void RemoveObserver(OverlayPresentationContextObserver* observer) override;
  UIPresentationCapabilities GetPresentationCapabilities() const override;
  bool CanShowUIForRequest(
      OverlayRequest* request,
      UIPresentationCapabilities capabilities) const override;
  bool CanShowUIForRequest(OverlayRequest* request) const override;
  void ShowOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request,
                     OverlayPresentationCallback presentation_callback,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request) override;
  void CancelOverlayUI(OverlayPresenter* presenter,
                       OverlayRequest* request) override;

 private:
  // Struct used to store state for the fake presentation context.
  struct FakeUIState {
    FakeUIState();
    ~FakeUIState();

    PresentationState presentation_state = PresentationState::kNotPresented;
    OverlayPresentationCallback presentation_callback;
    OverlayDismissalCallback dismissal_callback;
  };

  // The UI states for each request.
  std::map<OverlayRequest*, FakeUIState> states_;

  UIPresentationCapabilities capabilities_ =
      static_cast<UIPresentationCapabilities>(
          UIPresentationCapabilities::kContained |
          UIPresentationCapabilities::kPresented);

  base::ObserverList<OverlayPresentationContextObserver,
                     /* check_empty= */ true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
