// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_UI_STATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_UI_STATE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_dismissal_callback.h"
#include "ios/chrome/browser/overlays/public/overlay_presentation_callback.h"

@class OverlayRequestCoordinator;
class OverlayRequest;

// Container holding information about the state of overlay UI for a given
// request.
class OverlayRequestUIState {
 public:
  explicit OverlayRequestUIState(OverlayRequest* request);
  ~OverlayRequestUIState();

  // Called when the OverlayPresenter requests the presentation of |request_|.
  // This may or may not correspond with an OverlayUIWasPresented() if the
  // presentation context is inactive.  |presentation_callback| and
  // |dismissal_callback| are stored in the state, and will be executed when
  // OverlayUIWasPresented() and OverlayUIWasDismissed() are called.
  void OverlayPresentionRequested(
      OverlayPresentationCallback presentation_callback,
      OverlayDismissalCallback dismissal_callback);

  // Notifies the state that the UI is about to be presented using
  // |coordinator|.
  void OverlayUIWillBePresented(OverlayRequestCoordinator* coordinator);

  // Notifies the state that the UI was presented.
  void OverlayUIWasPresented();

  // Notifies the state the the UI was dismissed.
  void OverlayUIWasDismissed();

  // Accessors.
  OverlayRequestCoordinator* coordinator() const { return coordinator_; }
  bool has_ui_been_presented() const { return has_ui_been_presented_; }
  bool has_callback() const { return !dismissal_callback_.is_null(); }
  OverlayDismissalReason dismissal_reason() const { return dismissal_reason_; }
  void set_dismissal_reason(OverlayDismissalReason dismissal_reason) {
    dismissal_reason_ = dismissal_reason;
  }

 private:
  OverlayRequest* request_ = nullptr;
  OverlayRequestCoordinator* coordinator_ = nil;
  bool has_ui_been_presented_ = false;
  OverlayPresentationCallback presentation_callback_;
  OverlayDismissalReason dismissal_reason_ =
      OverlayDismissalReason::kUserInteraction;
  OverlayDismissalCallback dismissal_callback_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_UI_STATE_H_
