// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state_init.h"

namespace blink {

XRWorldTrackingState::XRWorldTrackingState(
    XRWorldTrackingStateInit* world_tracking_state_init) {
  if (world_tracking_state_init &&
      world_tracking_state_init->hasPlaneDetectionState()) {
    plane_detection_state_ = MakeGarbageCollected<XRPlaneDetectionState>(
        world_tracking_state_init->planeDetectionState());
  } else {
    plane_detection_state_ = MakeGarbageCollected<XRPlaneDetectionState>();
  }
}

void XRWorldTrackingState::Trace(blink::Visitor* visitor) {
  visitor->Trace(plane_detection_state_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
