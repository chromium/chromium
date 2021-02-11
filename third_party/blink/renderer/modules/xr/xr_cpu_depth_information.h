// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CPU_DEPTH_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CPU_DEPTH_INFORMATION_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

namespace gfx {
class Size;
class Transform;
}  // namespace gfx

namespace blink {

class ExceptionState;
class XRFrame;

class XRCPUDepthInformation final : public XRDepthInformation {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRCPUDepthInformation(
      const XRFrame* xr_frame,
      const gfx::Size& size,
      const gfx::Transform& norm_texture_from_norm_view,
      float raw_value_to_meters,
      DOMUint16Array* data);

  DOMUint16Array* data(ExceptionState& exception_state) const;

  float getDepthInMeters(float x,
                         float y,
                         ExceptionState& exception_state) const;

  void Trace(Visitor* visitor) const override;

 private:
  const Member<DOMUint16Array> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CPU_DEPTH_INFORMATION_H_
