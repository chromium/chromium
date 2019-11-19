// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session) : session_(session) {}

XRSpace::~XRSpace() = default;

std::unique_ptr<TransformationMatrix> XRSpace::MojoFromSpace() {
  // The base XRSpace does not have any relevant information, so can't determine
  // a transform here.
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::DefaultViewerPose() {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::SpaceFromMojo(
    const TransformationMatrix& mojo_from_viewer) {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::SpaceFromViewer(
    const TransformationMatrix& mojo_from_viewer) {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::SpaceFromInputForViewer(
    const TransformationMatrix& mojo_from_input,
    const TransformationMatrix& mojo_from_viewer) {
  return nullptr;
}

TransformationMatrix XRSpace::OriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

TransformationMatrix XRSpace::InverseOriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

XRPose* XRSpace::getPose(XRSpace* other_space,
                         const TransformationMatrix* base_pose_matrix) {
  std::unique_ptr<TransformationMatrix> mojo_from_space = MojoFromSpace();
  if (!mojo_from_space) {
    return nullptr;
  }

  // Rigid transforms should always be invertible.
  DCHECK(mojo_from_space->IsInvertible());
  TransformationMatrix space_from_mojo = mojo_from_space->Inverse();

  std::unique_ptr<TransformationMatrix> mojo_from_other =
      other_space->MojoFromSpace();
  if (!mojo_from_other) {
    return nullptr;
  }

  // TODO(crbug.com/969133): Update how EmulatedPosition is determined here once
  // spec issue https://github.com/immersive-web/webxr/issues/534 has been
  // resolved.
  TransformationMatrix space_from_other =
      space_from_mojo.Multiply(*mojo_from_other);
  return MakeGarbageCollected<XRPose>(space_from_other,
                                      session()->EmulatedPosition());
}

std::unique_ptr<TransformationMatrix>
XRSpace::SpaceFromViewerWithDefaultAndOffset(
    const TransformationMatrix* mojo_from_viewer) {
  std::unique_ptr<TransformationMatrix> space_from_viewer;

  // If we don't have a valid base pose, request the reference space's default
  // viewer pose. Most common when tracking is lost.
  if (mojo_from_viewer) {
    space_from_viewer = SpaceFromViewer(*mojo_from_viewer);
  } else {
    space_from_viewer = DefaultViewerPose();
  }

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!space_from_viewer || !space_from_viewer->IsInvertible()) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so that
  // things like teleportation works.
  //
  // This is offset_from_viewer = offset_from_space * space_from_viewer,
  // where offset_from_viewer = inverse(viewer_from_offset).
  // TODO(https://crbug.com/1008466): move originOffset to separate class?
  return std::make_unique<TransformationMatrix>(
      InverseOriginOffsetMatrix().Multiply(*space_from_viewer));
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

base::Optional<XRNativeOriginInformation> XRSpace::NativeOrigin() const {
  return base::nullopt;
}

void XRSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
