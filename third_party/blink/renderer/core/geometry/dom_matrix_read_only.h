// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GEOMETRY_DOM_MATRIX_READ_ONLY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GEOMETRY_DOM_MATRIX_READ_ONLY_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/string_or_unrestricted_double_sequence.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_2d_init.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class DOMMatrix;
class DOMMatrixInit;
class DOMPoint;
class DOMPointInit;

class CORE_EXPORT DOMMatrixReadOnly : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMMatrixReadOnly* Create(ExecutionContext*, ExceptionState&);
  static DOMMatrixReadOnly* Create(ExecutionContext*,
                                   StringOrUnrestrictedDoubleSequence&,
                                   ExceptionState&);
  static DOMMatrixReadOnly* fromFloat32Array(NotShared<DOMFloat32Array>,
                                             ExceptionState&);
  static DOMMatrixReadOnly* fromFloat64Array(NotShared<DOMFloat64Array>,
                                             ExceptionState&);
  static DOMMatrixReadOnly* fromMatrix(DOMMatrixInit*, ExceptionState&);
  static DOMMatrixReadOnly* fromMatrix2D(DOMMatrix2DInit*, ExceptionState&);
  static DOMMatrixReadOnly* CreateForSerialization(double[], int size);

  DOMMatrixReadOnly() = default;
  DOMMatrixReadOnly(const String&, ExceptionState&);
  DOMMatrixReadOnly(const TransformationMatrix&, bool is2d = true);

  template <typename T>
  DOMMatrixReadOnly(T sequence, int size) {
    if (size == 6) {
      matrix_.SetMatrix(sequence[0], sequence[1], sequence[2], sequence[3],
                        sequence[4], sequence[5]);
      is2d_ = true;
    } else if (size == 16) {
      matrix_.SetMatrix(sequence[0], sequence[1], sequence[2], sequence[3],
                        sequence[4], sequence[5], sequence[6], sequence[7],
                        sequence[8], sequence[9], sequence[10], sequence[11],
                        sequence[12], sequence[13], sequence[14], sequence[15]);
      is2d_ = false;
    } else {
      NOTREACHED();
    }
  }
  ~DOMMatrixReadOnly() override;

  double a() const { return matrix_.M11(); }
  double b() const { return matrix_.M12(); }
  double c() const { return matrix_.M21(); }
  double d() const { return matrix_.M22(); }
  double e() const { return matrix_.M41(); }
  double f() const { return matrix_.M42(); }

  double m11() const { return matrix_.M11(); }
  double m12() const { return matrix_.M12(); }
  double m13() const { return matrix_.M13(); }
  double m14() const { return matrix_.M14(); }
  double m21() const { return matrix_.M21(); }
  double m22() const { return matrix_.M22(); }
  double m23() const { return matrix_.M23(); }
  double m24() const { return matrix_.M24(); }
  double m31() const { return matrix_.M31(); }
  double m32() const { return matrix_.M32(); }
  double m33() const { return matrix_.M33(); }
  double m34() const { return matrix_.M34(); }
  double m41() const { return matrix_.M41(); }
  double m42() const { return matrix_.M42(); }
  double m43() const { return matrix_.M43(); }
  double m44() const { return matrix_.M44(); }

  bool is2D() const;
  bool isIdentity() const;

  DOMMatrix* multiply(DOMMatrixInit*, ExceptionState&);
  DOMMatrix* translate(double tx = 0, double ty = 0, double tz = 0);
  DOMMatrix* scale(double sx = 1);
  DOMMatrix* scale(double sx,
                   double sy,
                   double sz = 1,
                   double ox = 0,
                   double oy = 0,
                   double oz = 0);
  DOMMatrix* scaleNonUniform(double sx = 1, double sy = 1);
  DOMMatrix* scale3d(double scale = 1,
                     double ox = 0,
                     double oy = 0,
                     double oz = 0);
  DOMMatrix* rotate(double rot_x);
  DOMMatrix* rotate(double rot_x, double rot_y);
  DOMMatrix* rotate(double rot_x, double rot_y, double rot_z);
  DOMMatrix* rotateFromVector(double x, double y);
  DOMMatrix* rotateAxisAngle(double x = 0,
                             double y = 0,
                             double z = 0,
                             double angle = 0);
  DOMMatrix* skewX(double sx);
  DOMMatrix* skewY(double sy);
  DOMMatrix* flipX();
  DOMMatrix* flipY();
  DOMMatrix* inverse();

  DOMPoint* transformPoint(const DOMPointInit*);

  NotShared<DOMFloat32Array> toFloat32Array() const;
  NotShared<DOMFloat64Array> toFloat64Array() const;

  const String toString(ExceptionState&) const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  const TransformationMatrix& Matrix() const { return matrix_; }

  AffineTransform GetAffineTransform() const;

  void Trace(blink::Visitor* visitor) override {
    ScriptWrappable::Trace(visitor);
  }

 protected:
  void SetMatrixValueFromString(const ExecutionContext*,
                                const String&,
                                ExceptionState&);

  static bool ValidateAndFixup2D(DOMMatrix2DInit*);
  static bool ValidateAndFixup(DOMMatrixInit*, ExceptionState&);
  TransformationMatrix matrix_;
  bool is2d_;
};

}  // namespace blink

#endif
