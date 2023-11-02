// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZOOM_ZOOM_EVENT_MANAGER_OBSERVER_H_
#define COMPONENTS_ZOOM_ZOOM_EVENT_MANAGER_OBSERVER_H_

namespace zoom {

class ZoomEventManagerObserver {
 public:
  // TODO(wjmaclean): convert existing ZoomLevelChangedCallbacks to be
  // observers.
  virtual void OnZoomLevelChanged() {}
  virtual void OnDefaultZoomLevelChanged() {}

 protected:
  virtual ~ZoomEventManagerObserver() {}
};

}  // namespace zoom

#endif  // COMPONENTS_ZOOM_ZOOM_EVENT_MANAGER_OBSERVER_H_
