// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_

#include <memory>

#include "base/optional.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class XRRigidTransform;
class XRSession;
class XRSpace;

class XRPlane : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Orientation { kHorizontal, kVertical };

  XRPlane(uint64_t id,
          XRSession* session,
          const device::mojom::blink::XRPlaneDataPtr& plane_data,
          double timestamp);
  XRPlane(uint64_t id,
          XRSession* session,
          const base::Optional<Orientation>& orientation,
          const TransformationMatrix& pose_matrix,
          const HeapVector<Member<DOMPointReadOnly>>& polygon,
          double timestamp);

  uint64_t id() const;

  XRSpace* planeSpace() const;

  TransformationMatrix poseMatrix() const;

  String orientation() const;
  HeapVector<Member<DOMPointReadOnly>> polygon() const;
  double lastChangedTime() const;

  ScriptPromise createAnchor(ScriptState* script_state,
                             XRRigidTransform* initial_pose,
                             XRSpace* space,
                             ExceptionState& exception_state);

  // Updates plane data from passed in |plane_data|. The resulting instance
  // should be equivalent to the instance that would be create by calling
  // XRPlane(plane_data).
  void Update(const device::mojom::blink::XRPlaneDataPtr& plane_data,
              double timestamp);

  void Trace(blink::Visitor* visitor) override;

 private:
  const uint64_t id_;
  HeapVector<Member<DOMPointReadOnly>> polygon_;
  base::Optional<Orientation> orientation_;

  // Plane center's pose in device (mojo) space.
  std::unique_ptr<TransformationMatrix> pose_matrix_;

  Member<XRSession> session_;

  double last_changed_time_;

  // Cached plane space - it will be created by `planeSpace()` if it's not set.
  mutable Member<XRSpace> plane_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_
