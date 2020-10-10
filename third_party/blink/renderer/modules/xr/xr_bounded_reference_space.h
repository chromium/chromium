// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRBoundedReferenceSpace final : public XRReferenceSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRBoundedReferenceSpace(XRSession*);
  XRBoundedReferenceSpace(XRSession*, XRRigidTransform*);
  ~XRBoundedReferenceSpace() override;

  base::Optional<TransformationMatrix> MojoFromNative() override;

  HeapVector<Member<DOMPointReadOnly>> boundsGeometry();

  void Trace(Visitor*) const override;

  void OnReset() override;

 private:
  XRBoundedReferenceSpace* cloneWithOriginOffset(
      XRRigidTransform* origin_offset) override;

  void EnsureUpdated();

  HeapVector<Member<DOMPointReadOnly>> offset_bounds_geometry_;
  std::unique_ptr<TransformationMatrix> mojo_from_bounded_native_;
  unsigned int stage_parameters_id_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_
