// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(XRSession* session,
                           const TransformationMatrix& pose_model_matrix)
    : XRPose(pose_model_matrix, session->EmulatedPosition()) {
  WTF::Vector<XRViewData>& view_data = session->views();

  // Snapshot the session's current views.
  for (XRViewData& view : view_data) {
    view.UpdatePoseMatrix(transform_->TransformMatrix());
    views_.push_back(MakeGarbageCollected<XRView>(session, view));
  }
}

void XRViewerPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(views_);
  XRPose::Trace(visitor);
}

}  // namespace blink
