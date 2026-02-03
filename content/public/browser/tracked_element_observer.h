// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACKED_ELEMENT_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_TRACKED_ELEMENT_OBSERVER_H_

#include "base/observer_list_types.h"
#include "cc/trees/tracked_element_bounds.h"
#include "content/common/content_export.h"

namespace content {

// An observer API implemented by classes interested in Tracked Element changes.
class CONTENT_EXPORT TrackedElementObserver : public base::CheckedObserver {
 public:
  // This method is invoked when the tracked element bounds have changed.
  virtual void OnTrackedElementBoundsChanged(
      const cc::TrackedElementBounds& bounds,
      float device_scale_factor) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACKED_ELEMENT_OBSERVER_H_
