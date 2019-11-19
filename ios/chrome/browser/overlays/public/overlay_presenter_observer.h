// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_OBSERVER_H_

#include "base/observer_list_types.h"

class OverlayPresenter;
class OverlayRequest;

// Observer interface for objects interested in overlay presentation events
// triggered by OverlayPresenter.
class OverlayPresenterObserver : public base::CheckedObserver {
 public:
  OverlayPresenterObserver() = default;

  // Called when |presenter| is about to show the overlay UI for |request|.
  virtual void WillShowOverlay(OverlayPresenter* presenter,
                               OverlayRequest* request) {}

  // Called when |presenter| has finished showing the overlay UI for
  // |request|.
  virtual void DidShowOverlay(OverlayPresenter* presenter,
                              OverlayRequest* request) {}

  // Called when |presenter| is finished dismissing its overlay UI.
  virtual void DidHideOverlay(OverlayPresenter* presenter,
                              OverlayRequest* request) {}

  // Called when |presenter| is destroyed.
  virtual void OverlayPresenterDestroyed(OverlayPresenter* presenter) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_OBSERVER_H_
