// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeightMeters = 1.6;

XRReferenceSpace::Type XRReferenceSpace::StringToReferenceSpaceType(
    const String& reference_space_type) {
  if (reference_space_type == "viewer") {
    return XRReferenceSpace::Type::kTypeViewer;
  } else if (reference_space_type == "local") {
    return XRReferenceSpace::Type::kTypeLocal;
  } else if (reference_space_type == "local-floor") {
    return XRReferenceSpace::Type::kTypeLocalFloor;
  } else if (reference_space_type == "bounded-floor") {
    return XRReferenceSpace::Type::kTypeBoundedFloor;
  } else if (reference_space_type == "unbounded") {
    return XRReferenceSpace::Type::kTypeUnbounded;
  }
  NOTREACHED();
  return Type::kTypeViewer;
}

// origin offset starts as identity transform
XRReferenceSpace::XRReferenceSpace(XRSession* session, Type type)
    : XRReferenceSpace(session,
                       MakeGarbageCollected<XRRigidTransform>(nullptr, nullptr),
                       type) {}

XRReferenceSpace::XRReferenceSpace(XRSession* session,
                                   XRRigidTransform* origin_offset,
                                   Type type)
    : XRSpace(session), origin_offset_(origin_offset), type_(type) {}

XRReferenceSpace::~XRReferenceSpace() = default;

XRPose* XRReferenceSpace::getPose(
    XRSpace* other_space,
    const TransformationMatrix* mojo_from_viewer) {
  if (type_ == Type::kTypeViewer) {
    std::unique_ptr<TransformationMatrix> other_offsetspace_from_viewer =
        other_space->SpaceFromViewerWithDefaultAndOffset(mojo_from_viewer);
    if (!other_offsetspace_from_viewer) {
      return nullptr;
    }

    auto viewer_from_offset = OriginOffsetMatrix();

    auto other_offsetspace_from_offset =
        *other_offsetspace_from_viewer * viewer_from_offset;

    return MakeGarbageCollected<XRPose>(other_offsetspace_from_offset,
                                        session()->EmulatedPosition());
  } else {
    return XRSpace::getPose(other_space, mojo_from_viewer);
  }
}

void XRReferenceSpace::SetFloorFromMojo() {
  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stage_parameters) {
    // Use the transform given by xrDisplayInfo's stage_parameters if available.
    floor_from_mojo_ = std::make_unique<TransformationMatrix>(
        display_info->stage_parameters->standing_transform.matrix());
  } else {
    // Otherwise, create a transform based on the default emulated height.
    floor_from_mojo_ = std::make_unique<TransformationMatrix>();
    floor_from_mojo_->Translate3d(0, kDefaultEmulationHeightMeters, 0);
  }

  display_info_id_ = session()->DisplayInfoPtrId();
}

// Returns a default viewer pose if no actual viewer pose is available. Only
// applicable to viewer reference spaces.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::DefaultViewerPose() {
  // A viewer reference space always returns an identity matrix.
  return type_ == Type::kTypeViewer ? std::make_unique<TransformationMatrix>()
                                    : nullptr;
}

std::unique_ptr<TransformationMatrix> XRReferenceSpace::SpaceFromMojo(
    const TransformationMatrix& mojo_from_viewer) {
  switch (type_) {
    case Type::kTypeLocal:
      // Currently 'local' space is equivalent to mojo space.
      return std::make_unique<TransformationMatrix>();
    case Type::kTypeLocalFloor:
      // Currently all base poses are 'local' space, so use of 'local-floor'
      // reference spaces requires adjustment. Ideally the service will
      // eventually provide poses in the requested space directly, avoiding the
      // need to do additional transformation here.

      // Check first to see if the xrDisplayInfo has updated since the last
      // call. If so, update the floor-level transform.
      if (display_info_id_ != session()->DisplayInfoPtrId())
        SetFloorFromMojo();
      return std::make_unique<TransformationMatrix>(*floor_from_mojo_);
    case Type::kTypeViewer:
      // Return viewer_from_mojo which is the inverse of mojo_from_viewer.
      return std::make_unique<TransformationMatrix>(mojo_from_viewer.Inverse());
    case Type::kTypeUnbounded:
      // For now we assume that poses returned by systems that support unbounded
      // reference spaces are already in the correct space. Return an identity.
      return std::make_unique<TransformationMatrix>();
    case Type::kTypeBoundedFloor:
      NOTREACHED() << "kTypeBoundedFloor should be handled by subclass";
      break;
  }

  return nullptr;
}

// Returns the refspace-from-viewerspace transform, corresponding to the pose of
// the viewer in this space. This takes the mojo_from_viewer transform (viewer
// pose in mojo space) as input, and left-multiplies space_from_mojo onto that.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::SpaceFromViewer(
    const TransformationMatrix& mojo_from_viewer) {
  if (type_ == Type::kTypeViewer) {
    // Special case for viewer space, always return an identity matrix
    // explicitly. In theory the default behavior of multiplying SpaceFromMojo *
    // MojoFromViewer would be equivalent, but that would likely return an
    // almost-identity due to rounding errors.
    return std::make_unique<TransformationMatrix>();
  }

  // Return space_from_viewer = space_from_mojo * mojo_from_viewer
  auto space_from_viewer = SpaceFromMojo(mojo_from_viewer);
  if (!space_from_viewer)
    return nullptr;
  space_from_viewer->Multiply(mojo_from_viewer);
  return space_from_viewer;
}

std::unique_ptr<TransformationMatrix> XRReferenceSpace::SpaceFromInputForViewer(
    const TransformationMatrix& mojo_from_input,
    const TransformationMatrix& mojo_from_viewer) {
  // Return space_from_input = space_from_mojo * mojo_from_input
  auto space_from_input = SpaceFromMojo(mojo_from_viewer);
  if (!space_from_input)
    return nullptr;
  space_from_input->Multiply(mojo_from_input);
  return space_from_input;
}

std::unique_ptr<TransformationMatrix> XRReferenceSpace::MojoFromSpace() {
  // XRReferenceSpace doesn't do anything special with the base pose, but
  // derived reference spaces (bounded, unbounded, stationary, etc.) have their
  // own custom behavior.

  // Calculate the offset space's pose (including originOffset) in mojo
  // space.
  TransformationMatrix identity;
  std::unique_ptr<TransformationMatrix> mojo_from_offsetspace =
      SpaceFromViewer(identity);

  if (!mojo_from_offsetspace) {
    // Transform wasn't possible.
    return nullptr;
  }

  // Must account for position and orientation defined by origin offset.
  // Result is mojo_from_offset = mojo_from_ref * ref_from_offset,
  // where ref_from_offset is originOffset's transform matrix.
  // TODO(https://crbug.com/1008466): move originOffset to separate class?
  mojo_from_offsetspace->Multiply(origin_offset_->TransformMatrix());
  return mojo_from_offsetspace;
}

TransformationMatrix XRReferenceSpace::OriginOffsetMatrix() {
  return origin_offset_->TransformMatrix();
}

TransformationMatrix XRReferenceSpace::InverseOriginOffsetMatrix() {
  return origin_offset_->InverseTransformMatrix();
}

XRReferenceSpace::Type XRReferenceSpace::GetType() const {
  return type_;
}

XRReferenceSpace* XRReferenceSpace::getOffsetReferenceSpace(
    XRRigidTransform* additional_offset) {
  auto matrix =
      OriginOffsetMatrix().Multiply(additional_offset->TransformMatrix());

  auto* result_transform = MakeGarbageCollected<XRRigidTransform>(matrix);
  return cloneWithOriginOffset(result_transform);
}

XRReferenceSpace* XRReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) {
  return MakeGarbageCollected<XRReferenceSpace>(this->session(), origin_offset,
                                                type_);
}

base::Optional<XRNativeOriginInformation> XRReferenceSpace::NativeOrigin()
    const {
  return XRNativeOriginInformation::Create(this);
}

void XRReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(origin_offset_);
  XRSpace::Trace(visitor);
}

void XRReferenceSpace::OnReset() {
  if (type_ != Type::kTypeViewer) {
    DispatchEvent(
        *XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
  }
}

}  // namespace blink
