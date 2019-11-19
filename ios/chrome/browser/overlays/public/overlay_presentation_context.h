// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_

#include "ios/chrome/browser/overlays/public/overlay_dismissal_callback.h"
#include "ios/chrome/browser/overlays/public/overlay_presentation_callback.h"

class OverlayPresenter;
class OverlayRequest;
class OverlayPresentationContextObserver;

// Object that handles presenting the overlay UI for OverlayPresenter.
class OverlayPresentationContext {
 public:
  OverlayPresentationContext() = default;
  virtual ~OverlayPresentationContext() = default;

  // Adds and removes |observer|.
  virtual void AddObserver(OverlayPresentationContextObserver* observer) = 0;
  virtual void RemoveObserver(OverlayPresentationContextObserver* observer) = 0;

  // Enum describing the current capabilities of the presentation context.
  enum UIPresentationCapabilities {
    // The context cannot show any overlay UI.
    kNone = 0,
    // The context can show overlay UI that is contained as a child to its
    // backing UIViewController.
    kContained = 1 << 0,
    // The context can show overlay UI that is presented upon its backing
    // UIViewController.
    kPresented = 1 << 1,
  };

  // Returns the context's current presentation capabilities.  Overlay UI should
  // not be shown in this context if CanShowUIForRequest() returns false for
  // the current capabilities.
  virtual UIPresentationCapabilities GetPresentationCapabilities() const = 0;

  // Returns whether the presentation context can show the UI for |request|
  // while it has |capabilities|.
  virtual bool CanShowUIForRequest(
      OverlayRequest* request,
      UIPresentationCapabilities capabilities) const = 0;

  // Returns whether the presentation context supports showing the UI for
  // |request| with its current presentation capabilities.
  virtual bool CanShowUIForRequest(OverlayRequest* request) const = 0;

  // Called by |presenter| to show the overlay UI for |request|.
  // |presentation_callback| must be called when the UI is finished being
  // presented. |dismissal_callback| must be stored and called whenever the UI
  // is finished being dismissed for user interaction, hiding, or cancellation.
  virtual void ShowOverlayUI(OverlayPresenter* presenter,
                             OverlayRequest* request,
                             OverlayPresentationCallback presentation_callback,
                             OverlayDismissalCallback dismissal_callback) = 0;

  // Called by |presenter| to hide the overlay UI for |request|.  Hidden
  // overlays may be shown again, so they should be kept in memory or
  // serialized so that the state can be restored if shown again.  When hiding
  // an overlay, the presented UI must be dismissed, and the overlay's
  // dismissal callback must must be executed upon the dismissal's completion.
  virtual void HideOverlayUI(OverlayPresenter* presenter,
                             OverlayRequest* request) = 0;

  // Called by |presenter| to cancel the overlay UI for |request|.  If the UI
  // is presented, it should be dismissed and the dismissal callback should be
  // executed upon the dismissal's completion.  Otherwise, any state
  // corresponding to any hidden overlays should be cleaned up.
  virtual void CancelOverlayUI(OverlayPresenter* presenter,
                               OverlayRequest* request) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_
