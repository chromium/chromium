// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include <sstream>
#include <string>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

using ReferenceSpaceType = device::mojom::blink::XRReferenceSpaceType;

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeightMeters = -1.6;

ReferenceSpaceType XRReferenceSpace::V8EnumToReferenceSpaceType(
    V8XRReferenceSpaceType::Enum reference_space_type) {
  switch (reference_space_type) {
    case V8XRReferenceSpaceType::Enum::kViewer:
      return ReferenceSpaceType::kViewer;
    case V8XRReferenceSpaceType::Enum::kLocal:
      return ReferenceSpaceType::kLocal;
    case V8XRReferenceSpaceType::Enum::kLocalFloor:
      return ReferenceSpaceType::kLocalFloor;
    case V8XRReferenceSpaceType::Enum::kBoundedFloor:
      return ReferenceSpaceType::kBoundedFloor;
    case V8XRReferenceSpaceType::Enum::kUnbounded:
      return ReferenceSpaceType::kUnbounded;
  }
}

// origin offset starts as identity transform
XRReferenceSpace::XRReferenceSpace(XRSession* session, ReferenceSpaceType type)
    : XRReferenceSpace(session,
                       MakeGarbageCollected<XRRigidTransform>(nullptr, nullptr),
                       type) {}

XRReferenceSpace::XRReferenceSpace(XRSession* session,
                                   XRRigidTransform* origin_offset,
                                   ReferenceSpaceType type)
    : XRSpace(session), origin_offset_(origin_offset), type_(type) {}

XRReferenceSpace::~XRReferenceSpace() = default;

XRPose* XRReferenceSpace::getPose(const XRSpace* other_space) const {
  if (type_ == ReferenceSpaceType::kViewer) {
    std::optional<gfx::Transform> other_offset_from_viewer =
        other_space->OffsetFromViewer();
    if (!other_offset_from_viewer) {
      return nullptr;
    }

    auto viewer_from_offset = NativeFromOffsetMatrix();

    auto other_offset_from_offset =
        *other_offset_from_viewer * viewer_from_offset;

    return MakeGarbageCollected<XRPose>(other_offset_from_offset,
                                        session()->EmulatedPosition());
  } else {
    return XRSpace::getPose(other_space);
  }
}

void XRReferenceSpace::SetMojoFromFloor() const {
  const device::mojom::blink::VRStageParametersPtr& stage_parameters =
      session()->GetStageParameters();

  if (stage_parameters) {
    // Use the transform given by stage_parameters if available.
    mojo_from_floor_ =
        std::make_unique<gfx::Transform>(stage_parameters->mojo_from_floor);
  } else {
    mojo_from_floor_.reset();
  }

  stage_parameters_id_ = session()->StageParametersId();
}

std::optional<gfx::Transform> XRReferenceSpace::MojoFromNative() const {
  DVLOG(3) << __func__ << ": type_=" << type_;

  switch (type_) {
    case ReferenceSpaceType::kViewer:
    case ReferenceSpaceType::kLocal:
    case ReferenceSpaceType::kUnbounded: {
      // The session is the source of truth for latest state of the transform
      // between local & unbounded spaces and mojo space.
      auto mojo_from_native = session()->GetMojoFrom(type_);
      if (!mojo_from_native) {
        // The viewer reference space always has a default pose of identity if
        // it's not tracked; but for any other type if it's not locatable, we
        // return nullopt.
        return type_ == ReferenceSpaceType::kViewer
                   ? std::optional<gfx::Transform>(gfx::Transform{})
                   : std::nullopt;
      }

      return *mojo_from_native;
    }
    case ReferenceSpaceType::kLocalFloor: {
      // Check first to see if the stage_parameters has updated since the last
      // call. If so, update the floor-level transform.
      if (stage_parameters_id_ != session()->StageParametersId())
        SetMojoFromFloor();

      if (mojo_from_floor_) {
        return *mojo_from_floor_;
      }

      // If the floor-level transform is unavailable, try to use the default
      // transform based off of local space:
      auto mojo_from_local = session()->GetMojoFrom(ReferenceSpaceType::kLocal);
      if (!mojo_from_local) {
        return std::nullopt;
      }

      // local_from_floor-local transform corresponding to the default height.
      auto local_from_floor =
          gfx::Transform::MakeTranslation(0, kDefaultEmulationHeightMeters);

      return *mojo_from_local * local_from_floor;
    }
    case ReferenceSpaceType::kBoundedFloor: {
      NOTREACHED_IN_MIGRATION()
          << "kBoundedFloor should be handled by subclass";
      return std::nullopt;
    }
  }
}

std::optional<gfx::Transform> XRReferenceSpace::NativeFromViewer(
    const std::optional<gfx::Transform>& mojo_from_viewer) const {
  if (type_ == ReferenceSpaceType::kViewer) {
    // Special case for viewer space, always return an identity matrix
    // explicitly. In theory the default behavior of multiplying NativeFromMojo
    // onto MojoFromViewer would be equivalent, but that would likely return an
    // almost-identity due to rounding errors.
    return gfx::Transform();
  }

  if (!mojo_from_viewer)
    return std::nullopt;

  // Return native_from_viewer = native_from_mojo * mojo_from_viewer
  auto native_from_viewer = NativeFromMojo();
  if (!native_from_viewer)
    return std::nullopt;
  native_from_viewer->PreConcat(*mojo_from_viewer);
  return native_from_viewer;
}

gfx::Transform XRReferenceSpace::NativeFromOffsetMatrix() const {
  return origin_offset_->TransformMatrix();
}

gfx::Transform XRReferenceSpace::OffsetFromNativeMatrix() const {
  return origin_offset_->InverseTransformMatrix();
}

bool XRReferenceSpace::IsStationary() const {
  switch (type_) {
    case ReferenceSpaceType::kLocal:
    case ReferenceSpaceType::kLocalFloor:
    case ReferenceSpaceType::kBoundedFloor:
    case ReferenceSpaceType::kUnbounded:
      return true;
    case ReferenceSpaceType::kViewer:
      return false;
  }
}

ReferenceSpaceType XRReferenceSpace::GetType() const {
  return type_;
}

XRReferenceSpace* XRReferenceSpace::getOffsetReferenceSpace(
    XRRigidTransform* additional_offset) const {
  auto matrix = NativeFromOffsetMatrix() * additional_offset->TransformMatrix();

  auto* result_transform = MakeGarbageCollected<XRRigidTransform>(matrix);
  return cloneWithOriginOffset(result_transform);
}

XRReferenceSpace* XRReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) const {
  return MakeGarbageCollected<XRReferenceSpace>(this->session(), origin_offset,
                                                type_);
}

device::mojom::blink::XRNativeOriginInformationPtr
XRReferenceSpace::NativeOrigin() const {
  return device::mojom::blink::XRNativeOriginInformation::NewReferenceSpaceType(
      this->GetType());
}

std::string XRReferenceSpace::ToString() const {
  std::stringstream ss;

  ss << "XRReferenceSpace(type=" << type_ << ")";

  return ss.str();
}

void XRReferenceSpace::Trace(Visitor* visitor) const {
  visitor->Trace(origin_offset_);
  XRSpace::Trace(visitor);
}

void XRReferenceSpace::OnReset() {
  if (type_ != ReferenceSpaceType::kViewer) {
    // DispatchEvent inherited from core/dom/events/event_target.h isn't const.
    XRReferenceSpace* mutable_this = const_cast<XRReferenceSpace*>(this);
    mutable_this->DispatchEvent(
        *XRReferenceSpaceEvent::Create(event_type_names::kReset, mutable_this));
  }
}

}  // namespace blink
