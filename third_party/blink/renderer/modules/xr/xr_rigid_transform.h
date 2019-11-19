// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DOMPointInit;
class DOMPointReadOnly;
class ExceptionState;
class TransformationMatrix;

// MODULES_EXPORT is required for unit tests using XRRigidTransform (currently
// just xr_rigid_transform_test.cc) to build without linker errors.
class MODULES_EXPORT XRRigidTransform : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRRigidTransform(const TransformationMatrix&);
  XRRigidTransform(DOMPointInit*, DOMPointInit*);
  static XRRigidTransform* Create(DOMPointInit*,
                                  DOMPointInit*,
                                  ExceptionState&);

  ~XRRigidTransform() override = default;

  DOMPointReadOnly* position() const { return position_; }
  DOMPointReadOnly* orientation() const { return orientation_; }
  DOMFloat32Array* matrix();
  XRRigidTransform* inverse();

  TransformationMatrix InverseTransformMatrix();
  TransformationMatrix TransformMatrix();  // copies matrix_

  void Trace(blink::Visitor*) override;

 private:
  void DecomposeMatrix();
  void EnsureMatrix();
  void EnsureInverse();

  Member<DOMPointReadOnly> position_;
  Member<DOMPointReadOnly> orientation_;
  Member<XRRigidTransform> inverse_;
  Member<DOMFloat32Array> matrix_array_;
  std::unique_ptr<TransformationMatrix> matrix_;

  DISALLOW_COPY_AND_ASSIGN(XRRigidTransform);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
