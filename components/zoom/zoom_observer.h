// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZOOM_ZOOM_OBSERVER_H_
#define COMPONENTS_ZOOM_ZOOM_OBSERVER_H_

#include "components/zoom/zoom_controller.h"

namespace zoom {

// Interface for objects that wish to be notified of changes in ZoomController.
class ZoomObserver : public base::CheckedObserver {
 public:
  // Fired when the ZoomController is destructed. Observers should deregister
  // themselves from the ZoomObserver in this event handler. Note that
  // ZoomController::FromWebContents() returns nullptr at this point already.
  virtual void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) = 0;

  // Notification that the zoom percentage has changed.
  virtual void OnZoomChanged(const ZoomController::ZoomChangedEventData& data) {
  }
};

}  // namespace zoom

#endif  // COMPONENTS_ZOOM_ZOOM_OBSERVER_H_
