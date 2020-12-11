// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace blink {

class ExceptionState;
class XRFrame;
class XRRigidTransform;

class XRDepthInformation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRDepthInformation(const XRFrame* xr_frame,
                              const gfx::Size& size,
                              const gfx::Transform& norm_texture_from_norm_view,
                              DOMUint16Array* data);

  DOMUint16Array* data(ExceptionState& exception_state) const;

  uint32_t width(ExceptionState& exception_state) const;

  uint32_t height(ExceptionState& exception_state) const;

  XRRigidTransform* normTextureFromNormView(
      ExceptionState& exception_state) const;

  float getDepth(uint32_t column,
                 uint32_t row,
                 ExceptionState& exception_state) const;

  void Trace(Visitor* visitor) const override;

 private:
  const Member<const XRFrame> xr_frame_;

  const gfx::Size size_;

  const Member<DOMUint16Array> data_;
  const gfx::Transform norm_texture_from_norm_view_;

  // Helper to validate whether a frame is in a correct state. Should be invoked
  // before every member access. If the validation returns `false`, it means the
  // validation failed & an exception is going to be thrown and the rest of the
  // member access code should not run.
  bool ValidateFrame(ExceptionState& exception_state) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
