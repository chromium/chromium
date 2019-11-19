// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_test_utils.h"

namespace blink {

Vector<double> GetMatrixDataForTest(const TransformationMatrix& matrix) {
  Vector<double> data{matrix.M11(), matrix.M12(), matrix.M13(), matrix.M14(),
                      matrix.M21(), matrix.M22(), matrix.M23(), matrix.M24(),
                      matrix.M31(), matrix.M32(), matrix.M33(), matrix.M34(),
                      matrix.M41(), matrix.M42(), matrix.M43(), matrix.M44()};
  return data;
}

DOMPointInit* MakePointForTest(double x, double y, double z, double w) {
  DOMPointInit* point = DOMPointInit::Create();
  point->setX(x);
  point->setY(y);
  point->setZ(z);
  point->setW(w);
  return point;
}

}  // namespace blink
