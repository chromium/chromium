// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_

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

  std::unique_ptr<TransformationMatrix> DefaultViewerPose() override;
  std::unique_ptr<TransformationMatrix> SpaceFromMojo(
      const TransformationMatrix& mojo_from_viewer) override;

  HeapVector<Member<DOMPointReadOnly>> boundsGeometry();

  base::Optional<XRNativeOriginInformation> NativeOrigin() const override;

  void Trace(blink::Visitor*) override;

  void OnReset() override;

 private:
  XRBoundedReferenceSpace* cloneWithOriginOffset(
      XRRigidTransform* origin_offset) override;

  void EnsureUpdated();

  HeapVector<Member<DOMPointReadOnly>> bounds_geometry_;
  std::unique_ptr<TransformationMatrix> bounded_space_from_mojo_;
  unsigned int stage_parameters_id_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_
