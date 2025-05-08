// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_view_geometry.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class ExceptionState;
class XRView;
class XRRigidTransform;

class XRDepthInformation : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 protected:
  XRDepthInformation(
      const XRView* xr_view,
      const device::mojom::blink::XRViewGeometryPtr& view_geometry,
      const gfx::Size& size,
      const gfx::Transform& norm_depth_buffer_from_norm_view,
      float raw_value_to_meters);

  // Helper to validate whether a frame is in a correct state. Should be invoked
  // before every member access. If the validation returns `false`, it means the
  // validation failed & an exception is going to be thrown and the rest of the
  // member access code should not run.
  bool ValidateFrame(ExceptionState& exception_state) const;

 public:
  uint32_t width() const;
  uint32_t height() const;
  XRRigidTransform* normDepthBufferFromNormView() const;
  float rawValueToMeters() const;
  NotShared<DOMFloat32Array> projectionMatrix() const;
  XRRigidTransform* viewGeometryTransform() const;

  void Trace(Visitor* visitor) const override;

 protected:
  const Member<const XRView> xr_view_;

  const gfx::Size size_;

  const gfx::Transform norm_depth_buffer_from_norm_view_;
  const float raw_value_to_meters_;
  const std::optional<XRViewGeometry> view_geometry_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
