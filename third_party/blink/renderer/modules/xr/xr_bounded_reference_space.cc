// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {
namespace {

// Bounds must be a valid polygon (at least 3 vertices).
constexpr wtf_size_t kMinimumNumberOfBoundVertices = 3;

float RoundCm(float val) {
  // Float round will only round to the nearest whole number. In order to get
  // two decimal points of precision, we need to move the decimal out then
  // back.
  return std::round(val * 100) / 100;
}

Member<DOMPointReadOnly> RoundedDOMPoint(const gfx::Point3F& val) {
  return DOMPointReadOnly::Create(RoundCm(val.x()), RoundCm(val.y()),
                                  RoundCm(val.z()), 1.0);
}
}  // anonymous namespace

XRBoundedReferenceSpace::XRBoundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(
          session,
          device::mojom::blink::XRReferenceSpaceType::kBoundedFloor),
      offset_bounds_geometry_(
          MakeGarbageCollected<FrozenArray<DOMPointReadOnly>>()) {}

XRBoundedReferenceSpace::XRBoundedReferenceSpace(
    XRSession* session,
    XRRigidTransform* origin_offset)
    : XRReferenceSpace(
          session,
          origin_offset,
          device::mojom::blink::XRReferenceSpaceType::kBoundedFloor),
      offset_bounds_geometry_(
          MakeGarbageCollected<FrozenArray<DOMPointReadOnly>>()) {}

XRBoundedReferenceSpace::~XRBoundedReferenceSpace() = default;

void XRBoundedReferenceSpace::EnsureUpdated() const {
  // Check first to see if the stage parameters have updated since the last
  // call. We only need to update the transform and bounds if it has.
  if (stage_parameters_id_ == session()->StageParametersId())
    return;

  stage_parameters_id_ = session()->StageParametersId();

  const device::mojom::blink::VRStageParametersPtr& stage_parameters =
      session()->GetStageParameters();

  if (stage_parameters) {
    // Use the transform given by stage_parameters if available.
    mojo_from_bounded_native_ =
        std::make_unique<gfx::Transform>(stage_parameters->mojo_from_floor);

    // In order to ensure that the bounds continue to line up with the user's
    // physical environment we need to transform them from native to offset.
    // Bounds are provided in our native coordinate space.
    // TODO(https://crbug.com/1008466): move originOffset to separate class? If
    // yes, that class would need to apply a transform in the boundsGeometry
    // accessor.
    gfx::Transform offset_from_native = OffsetFromNativeMatrix();

    // We may not have bounds if we've lost tracking after being created.
    // Whether we have them or not, we need to clear the existing bounds.
    FrozenArray<DOMPointReadOnly>::VectorType offset_bounds_geometry;
    if (stage_parameters->bounds &&
        stage_parameters->bounds->size() >= kMinimumNumberOfBoundVertices) {
      for (const auto& bound : *(stage_parameters->bounds)) {
        gfx::Point3F p = offset_from_native.MapPoint(
            gfx::Point3F(bound.x(), 0.0, bound.z()));
        offset_bounds_geometry.push_back(RoundedDOMPoint(p));
      }
    }
    offset_bounds_geometry_ =
        MakeGarbageCollected<FrozenArray<DOMPointReadOnly>>(
            std::move(offset_bounds_geometry));
  } else {
    // If stage parameters aren't available set the transform to null, which
    // will subsequently cause this reference space to return null poses.
    mojo_from_bounded_native_.reset();
    offset_bounds_geometry_ =
        MakeGarbageCollected<FrozenArray<DOMPointReadOnly>>();
  }

  // DispatchEvent inherited from core/dom/events/event_target.h isn't const.
  XRBoundedReferenceSpace* mutable_this =
      const_cast<XRBoundedReferenceSpace*>(this);
  mutable_this->DispatchEvent(
      *XRReferenceSpaceEvent::Create(event_type_names::kReset, mutable_this));
}

std::optional<gfx::Transform> XRBoundedReferenceSpace::MojoFromNative() const {
  EnsureUpdated();

  if (!mojo_from_bounded_native_)
    return std::nullopt;

  return *mojo_from_bounded_native_;
}

const FrozenArray<DOMPointReadOnly>& XRBoundedReferenceSpace::boundsGeometry()
    const {
  EnsureUpdated();
  return *offset_bounds_geometry_.Get();
}

void XRBoundedReferenceSpace::Trace(Visitor* visitor) const {
  visitor->Trace(offset_bounds_geometry_);
  XRReferenceSpace::Trace(visitor);
}

void XRBoundedReferenceSpace::OnReset() {
  // Anything that would cause an external source to try to tell us that we've
  // been reset should have also updated the stage_parameters, and thus caused
  // us to reset via that mechanism instead.
}

XRBoundedReferenceSpace* XRBoundedReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) const {
  return MakeGarbageCollected<XRBoundedReferenceSpace>(this->session(),
                                                       origin_offset);
}

}  // namespace blink
