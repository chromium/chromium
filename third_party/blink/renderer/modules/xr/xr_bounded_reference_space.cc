// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

namespace blink {

XRBoundedReferenceSpace::XRBoundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session, Type::kTypeBoundedFloor) {}

XRBoundedReferenceSpace::XRBoundedReferenceSpace(
    XRSession* session,
    XRRigidTransform* origin_offset)
    : XRReferenceSpace(session, origin_offset, Type::kTypeBoundedFloor) {}

XRBoundedReferenceSpace::~XRBoundedReferenceSpace() = default;

// No default pose for bounded reference spaces.
std::unique_ptr<TransformationMatrix>
XRBoundedReferenceSpace::DefaultViewerPose() {
  return nullptr;
}

void XRBoundedReferenceSpace::EnsureUpdated() {
  // Check first to see if the stage parameters have updated since the last
  // call. We only need to update the transform and bounds if it has.
  if (stage_parameters_id_ == session()->StageParametersId())
    return;

  stage_parameters_id_ = session()->StageParametersId();

  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stage_parameters) {
    // Use the transform given by xrDisplayInfo's stage_parameters if available.
    bounded_space_from_mojo_ = std::make_unique<TransformationMatrix>(
        display_info->stage_parameters->standing_transform.matrix());

    // In order to ensure that the bounds continue to line up with the user's
    // physical environment we need to transform by the inverse of the
    // originOffset. TODO(https://crbug.com/1008466): move originOffset to
    // separate class? If yes, that class would need to apply a transform
    // in the boundsGeometry accessor.
    TransformationMatrix bounds_transform = InverseOriginOffsetMatrix();

    if (display_info->stage_parameters->bounds) {
      bounds_geometry_.clear();

      for (const auto& bound : *(display_info->stage_parameters->bounds)) {
        FloatPoint3D p =
            bounds_transform.MapPoint(FloatPoint3D(bound.X(), 0.0, bound.Z()));
        bounds_geometry_.push_back(
            DOMPointReadOnly::Create(p.X(), p.Y(), p.Z(), 1.0));
      }
    } else {
      double hx = display_info->stage_parameters->size_x * 0.5;
      double hz = display_info->stage_parameters->size_z * 0.5;
      FloatPoint3D a = bounds_transform.MapPoint(FloatPoint3D(hx, 0.0, -hz));
      FloatPoint3D b = bounds_transform.MapPoint(FloatPoint3D(hx, 0.0, hz));
      FloatPoint3D c = bounds_transform.MapPoint(FloatPoint3D(-hx, 0.0, hz));
      FloatPoint3D d = bounds_transform.MapPoint(FloatPoint3D(-hx, 0.0, -hz));

      bounds_geometry_.clear();
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(a.X(), a.Y(), a.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(b.X(), b.Y(), b.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(c.X(), c.Y(), c.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(d.X(), d.Y(), d.Z(), 1.0));
    }
  } else {
    // If stage parameters aren't available set the transform to null, which
    // will subsequently cause this reference space to return null poses.
    bounded_space_from_mojo_.reset();
    bounds_geometry_.clear();
  }

  DispatchEvent(*XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
}

// Gets the pose of the mojo origin in this reference space, corresponding to a
// transform from mojo coordinates to reference space coordinates. Ideally in
// the future this reference space can be used without additional transforms,
// with the various XR backends returning poses already in the right space.
std::unique_ptr<TransformationMatrix> XRBoundedReferenceSpace::SpaceFromMojo(
    const TransformationMatrix& mojo_from_viewer) {
  EnsureUpdated();

  // If the reference space has a transform apply it to the base pose and return
  // that, otherwise return null.
  if (bounded_space_from_mojo_) {
    std::unique_ptr<TransformationMatrix> space_from_mojo(
        std::make_unique<TransformationMatrix>(*bounded_space_from_mojo_));
    return space_from_mojo;
  }
  return nullptr;
}

HeapVector<Member<DOMPointReadOnly>> XRBoundedReferenceSpace::boundsGeometry() {
  EnsureUpdated();
  return bounds_geometry_;
}

base::Optional<XRNativeOriginInformation>
XRBoundedReferenceSpace::NativeOrigin() const {
  return XRNativeOriginInformation::Create(this);
}

void XRBoundedReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounds_geometry_);
  XRReferenceSpace::Trace(visitor);
}

void XRBoundedReferenceSpace::OnReset() {
  // Anything that would cause an external source to try to tell us that we've
  // been reset should have also updated the stage_parameters, and thus caused
  // us to reset via that mechanism instead.
}

XRBoundedReferenceSpace* XRBoundedReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) {
  return MakeGarbageCollected<XRBoundedReferenceSpace>(this->session(),
                                                       origin_offset);
}

}  // namespace blink
