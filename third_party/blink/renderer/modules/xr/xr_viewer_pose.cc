// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(XRFrame* frame,
                           const gfx::Transform& ref_space_from_mojo,
                           const gfx::Transform& ref_space_from_viewer,
                           bool emulated_position)
    : XRPose(ref_space_from_viewer, emulated_position) {
  DVLOG(3) << __func__ << ": emulatedPosition()=" << emulatedPosition();

  const HeapVector<Member<XRViewData>>& view_data = frame->session()->views();

  // Snapshot the session's current views.
  HeapVector<Member<XRView>> views;
  for (XRViewData* view : view_data) {
    views.push_back(
        MakeGarbageCollected<XRView>(frame, view, ref_space_from_mojo));
  }
  views_ = MakeGarbageCollected<FrozenArray<XRView>>(std::move(views));
}

void XRViewerPose::Trace(Visitor* visitor) const {
  visitor->Trace(views_);
  XRPose::Trace(visitor);
}

}  // namespace blink
