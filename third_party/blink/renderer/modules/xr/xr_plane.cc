// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace {

const char kUnknownPlanePose[] = "Plane pose is unknown.";

}

namespace blink {

XRPlane::XRPlane(uint64_t id,
                 XRSession* session,
                 const device::mojom::blink::XRPlaneData& plane_data,
                 double timestamp)
    : XRPlane(id,
              session,
              mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
                  plane_data.orientation),
              mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(
                  plane_data.polygon),
              plane_data.mojo_from_plane,
              timestamp) {}

XRPlane::XRPlane(uint64_t id,
                 XRSession* session,
                 const base::Optional<Orientation>& orientation,
                 const HeapVector<Member<DOMPointReadOnly>>& polygon,
                 const base::Optional<device::Pose>& mojo_from_plane,
                 double timestamp)
    : id_(id),
      polygon_(polygon),
      orientation_(orientation),
      mojo_from_plane_(mojo_from_plane),
      session_(session),
      last_changed_time_(timestamp) {
  DVLOG(3) << __func__;
}

uint64_t XRPlane::id() const {
  return id_;
}

XRSpace* XRPlane::planeSpace() const {
  if (!plane_space_) {
    plane_space_ = MakeGarbageCollected<XRObjectSpace<XRPlane>>(session_, this);
  }

  return plane_space_;
}

base::Optional<TransformationMatrix> XRPlane::MojoFromObject() const {
  if (!mojo_from_plane_) {
    return base::nullopt;
  }

  return TransformationMatrix(mojo_from_plane_->ToTransform().matrix());
}

String XRPlane::orientation() const {
  if (orientation_) {
    switch (*orientation_) {
      case Orientation::kHorizontal:
        return "Horizontal";
      case Orientation::kVertical:
        return "Vertical";
    }
  }
  return "";
}

double XRPlane::lastChangedTime() const {
  return last_changed_time_;
}

HeapVector<Member<DOMPointReadOnly>> XRPlane::polygon() const {
  // Returns copy of a vector - by design. This way, JavaScript code could
  // store the state of the plane's polygon in frame N just by storing the
  // array (`let polygon = plane.polygon`) - the stored array won't be affected
  // by the changes to the plane that could happen in frames >N.
  return polygon_;
}

ScriptPromise XRPlane::createAnchor(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!session_->IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      XRSession::kAnchorsFeatureNotSupported);
    return {};
  }

  if (!mojo_from_plane_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kUnknownPlanePose);
    return {};
  }

  // Planes are not considered stationary for the purpose of anchor creation
  // (their poses may change dramatically on a frame-by-frame basis). Grab an
  // information about reference space that is well-suited for anchor creation
  // from session:
  base::Optional<XRSession::ReferenceSpaceInformation>
      reference_space_information = session_->GetStationaryReferenceSpace();

  if (!reference_space_information) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    return {};
  }

  const TransformationMatrix& mojo_from_space =
      reference_space_information->mojo_from_space;

  DCHECK(mojo_from_space.IsInvertible());

  auto space_from_mojo = mojo_from_space.Inverse();
  // We'll create an anchor located at the current plane's pose:
  auto space_from_anchor =
      space_from_mojo *
      TransformationMatrix(mojo_from_plane_->ToTransform().matrix());

  return session_->CreatePlaneAnchorHelper(
      script_state, space_from_anchor,
      reference_space_information->native_origin, id_, exception_state);
}

void XRPlane::Update(const device::mojom::blink::XRPlaneData& plane_data,
                     double timestamp) {
  DVLOG(3) << __func__;

  last_changed_time_ = timestamp;

  orientation_ = mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
      plane_data.orientation);

  mojo_from_plane_ = plane_data.mojo_from_plane;

  polygon_ =
      mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(plane_data.polygon);
}

void XRPlane::Trace(Visitor* visitor) const {
  visitor->Trace(polygon_);
  visitor->Trace(session_);
  visitor->Trace(plane_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
