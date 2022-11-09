// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_test_utils.h"

namespace blink {

Vector<double> GetMatrixDataForTest(const gfx::Transform& matrix) {
  Vector<double> data(16);
  matrix.GetColMajor(data.data());
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
