// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(XRFrame* frame,
                           const TransformationMatrix& pose_model_matrix)
    : XRPose(pose_model_matrix, frame->session()->EmulatedPosition()) {
  DVLOG(3) << __func__ << ": emulatedPosition()=" << emulatedPosition();

  const HeapVector<Member<XRViewData>>& view_data = frame->session()->views();

  bool camera_access_enabled = frame->session()->IsFeatureEnabled(
      device::mojom::XRSessionFeature::CAMERA_ACCESS);

  // Snapshot the session's current views.
  for (XRViewData* view : view_data) {
    view->UpdatePoseMatrix(transform_->TransformMatrix());
    XRView* xr_view = MakeGarbageCollected<XRView>(frame, view);
    views_.push_back(xr_view);
    if (camera_access_enabled) {
      camera_views_.push_back(xr_view);
    }
  }
}

void XRViewerPose::Trace(Visitor* visitor) const {
  visitor->Trace(views_);
  visitor->Trace(camera_views_);
  XRPose::Trace(visitor);
}

}  // namespace blink
