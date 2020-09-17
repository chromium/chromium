// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/transform.h"

namespace blink {

class ExceptionState;
class XRRigidTransform;

class XRDepthInformation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRDepthInformation(
      const device::mojom::blink::XRDepthData& depth_data);

  DOMUint16Array* data() const;

  uint32_t width() const;

  uint32_t height() const;

  XRRigidTransform* normTextureFromNormView() const;

  float getDepth(uint32_t column,
                 uint32_t row,
                 ExceptionState& exception_state) const;

  void Trace(Visitor* visitor) const override;

 private:
  uint32_t width_;
  uint32_t height_;

  Member<DOMUint16Array> data_;
  gfx::Transform norm_texture_from_norm_view_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
