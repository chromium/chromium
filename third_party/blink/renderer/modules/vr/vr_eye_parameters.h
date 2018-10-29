// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_EYE_PARAMETERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_EYE_PARAMETERS_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/vr/vr_field_of_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class VREyeParameters final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit VREyeParameters(const device::mojom::blink::VREyeParametersPtr&,
                           double render_scale);

  DOMFloat32Array* offset() const { return offset_; }
  VRFieldOfView* FieldOfView() const { return field_of_view_; }
  uint32_t renderWidth() const { return render_width_; }
  uint32_t renderHeight() const { return render_height_; }

  void Trace(blink::Visitor*) override;

 private:
  Member<DOMFloat32Array> offset_;
  Member<VRFieldOfView> field_of_view_;
  uint32_t render_width_;
  uint32_t render_height_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_EYE_PARAMETERS_H_
