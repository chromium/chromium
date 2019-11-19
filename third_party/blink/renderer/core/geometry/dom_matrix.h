// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GEOMETRY_DOM_MATRIX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GEOMETRY_DOM_MATRIX_H_

#include "third_party/blink/renderer/bindings/core/v8/string_or_unrestricted_double_sequence.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_2d_init.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_init.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class CORE_EXPORT DOMMatrix : public DOMMatrixReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMMatrix* Create();
  static DOMMatrix* Create(ExecutionContext*, ExceptionState&);
  static DOMMatrix* Create(ExecutionContext*,
                           StringOrUnrestrictedDoubleSequence&,
                           ExceptionState&);
  // TODO(fserb): double check those two bellow are needed:
  static DOMMatrix* Create(DOMMatrixReadOnly*,
                           ExceptionState& = ASSERT_NO_EXCEPTION);
  static DOMMatrix* Create(const SkMatrix44&, ExceptionState&);
  static DOMMatrix* fromFloat32Array(NotShared<DOMFloat32Array>,
                                     ExceptionState&);
  static DOMMatrix* fromFloat64Array(NotShared<DOMFloat64Array>,
                                     ExceptionState&);
  static DOMMatrix* fromMatrix(DOMMatrixInit*, ExceptionState&);
  static DOMMatrix* CreateForSerialization(double[], int size);

  DOMMatrix(const TransformationMatrix&, bool is2d = true);
  template <typename T>
  DOMMatrix(T sequence, int size);

  void setA(double value) { matrix_.SetM11(value); }
  void setB(double value) { matrix_.SetM12(value); }
  void setC(double value) { matrix_.SetM21(value); }
  void setD(double value) { matrix_.SetM22(value); }
  void setE(double value) { matrix_.SetM41(value); }
  void setF(double value) { matrix_.SetM42(value); }

  void setM11(double value) { matrix_.SetM11(value); }
  void setM12(double value) { matrix_.SetM12(value); }
  void setM13(double value) {
    matrix_.SetM13(value);
    SetIs2D(!value);
  }
  void setM14(double value) {
    matrix_.SetM14(value);
    SetIs2D(!value);
  }
  void setM21(double value) { matrix_.SetM21(value); }
  void setM22(double value) { matrix_.SetM22(value); }
  void setM23(double value) {
    matrix_.SetM23(value);
    SetIs2D(!value);
  }
  void setM24(double value) {
    matrix_.SetM24(value);
    SetIs2D(!value);
  }
  void setM31(double value) {
    matrix_.SetM31(value);
    SetIs2D(!value);
  }
  void setM32(double value) {
    matrix_.SetM32(value);
    SetIs2D(!value);
  }
  void setM33(double value) {
    matrix_.SetM33(value);
    SetIs2D(value == 1);
  }
  void setM34(double value) {
    matrix_.SetM34(value);
    SetIs2D(!value);
  }
  void setM41(double value) { matrix_.SetM41(value); }
  void setM42(double value) { matrix_.SetM42(value); }
  void setM43(double value) {
    matrix_.SetM43(value);
    SetIs2D(!value);
  }
  void setM44(double value) {
    matrix_.SetM44(value);
    SetIs2D(value == 1);
  }

  DOMMatrix* multiplySelf(DOMMatrixInit*, ExceptionState&);
  DOMMatrix* multiplySelf(const DOMMatrix& other_matrix);
  DOMMatrix* preMultiplySelf(DOMMatrixInit*, ExceptionState&);
  DOMMatrix* translateSelf(double tx = 0, double ty = 0, double tz = 0);
  DOMMatrix* scaleSelf(double sx = 1);
  DOMMatrix* scaleSelf(double sx,
                       double sy,
                       double sz = 1,
                       double ox = 0,
                       double oy = 0,
                       double oz = 0);
  DOMMatrix* scale3dSelf(double scale = 1,
                         double ox = 0,
                         double oy = 0,
                         double oz = 0);
  DOMMatrix* rotateSelf(double rot_x);
  DOMMatrix* rotateSelf(double rot_x, double rot_y);
  DOMMatrix* rotateSelf(double rot_x, double rot_y, double rot_z);
  DOMMatrix* rotateFromVectorSelf(double x, double y);
  DOMMatrix* rotateAxisAngleSelf(double x = 0,
                                 double y = 0,
                                 double z = 0,
                                 double angle = 0);
  DOMMatrix* skewXSelf(double sx = 0);
  DOMMatrix* skewYSelf(double sy = 0);
  DOMMatrix* perspectiveSelf(double p);
  DOMMatrix* invertSelf();

  DOMMatrix* setMatrixValue(const ExecutionContext*,
                            const String&,
                            ExceptionState&);

 private:
  void SetIs2D(bool value);
  void SetNAN();
};

}  // namespace blink

#endif
