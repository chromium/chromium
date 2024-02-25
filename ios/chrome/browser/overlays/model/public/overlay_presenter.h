// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_H_


#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"

class Browser;
class OverlayPresenterObserver;
class OverlayPresentationContext;

// OverlayPresenter handles the presentation of overlay UI for OverlayRequests
// added to the OverlayRequestQueues for WebStates in a Browser.
class OverlayPresenter {
 public:
  virtual ~OverlayPresenter() = default;

  // Retrieves the OverlayPresenter for `browser` that manages overlays at
  // `modality`, creating one if necessary.
  static OverlayPresenter* FromBrowser(Browser* browser,
                                       OverlayModality modality);

  // Returns the presenter's modality.
  virtual OverlayModality GetModality() const = 0;

  // Sets the presentation context in which to show overlay UI.  Upon being set,
  // the presenter will attempt to begin presenting overlay UI for the active
  // WebState in its Browser.
  virtual void SetPresentationContext(
      OverlayPresentationContext* presentation_context) = 0;

  // Adds and removes observers.
  virtual void AddObserver(OverlayPresenterObserver* observer) = 0;
  virtual void RemoveObserver(OverlayPresenterObserver* observer) = 0;

  // Whether overlay UI is currently shown in the presentation context.
  virtual bool IsShowingOverlayUI() const = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_H_
