// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_math_utils.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace device {

gfx::Transform MatrixFromTransformedPoints(const base::span<const float> uvs) {
  DCHECK_GE(uvs.size(), 6u);

  //
  // In order to compute the matrix, we need to solve the following 3 equations
  // for 6 unknowns:
  //
  //  | a b c |   | u |   | u' |
  //  | d e f | * | v | = | v' |
  //  | 0 0 1 |   | 1 |   | 1  |
  //
  //  where 3 (u', v') pairs are passed in as an input to the method, and (u,v)
  //  pairs are assumed to come from kInputCoordinatesForTransform.
  //
  // 1. From substituting point (0, 0) for (u,v), we get:
  //
  //  c = uvs[0]
  //  f = uvs[1]
  //
  // 2. From substituting point (1, 0) for (u,v), we get:
  //
  //  a + c = uvs[2]  ->  a = uvs[2] - uvs[0]
  //  d + f = uvs[3]  ->  d = uvs[3] - uvs[1]
  //
  // 3. From substituting point (0, 1) for (u,v), we get:
  //
  //  b + c = uvs[4]  ->  b = uvs[4] - uvs[0]
  //  e + f = uvs[5]  ->  e = uvs[5] - uvs[1]
  //

  DVLOG(3) << __func__ << ": uvs=[ " << uvs[0] << " , " << uvs[1] << " , "
           << uvs[2] << " , " << uvs[3] << " , " << uvs[4] << " , " << uvs[5]
           << " ]";

  // Assumes that |uvs| is the result of transforming the display coordinates
  // from kInputCoordinatesForTransform - size must match.
  DCHECK_EQ(uvs.size(), kInputCoordinatesForTransform.size());

  // Transform initializes to the identity matrix and then is modified by uvs.
  gfx::Transform result;
  result.set_rc(0, 0, uvs[2] - uvs[0]);
  result.set_rc(0, 1, uvs[4] - uvs[0]);
  result.set_rc(0, 3, uvs[0]);

  result.set_rc(1, 0, uvs[3] - uvs[1]);
  result.set_rc(1, 1, uvs[5] - uvs[1]);
  result.set_rc(1, 3, uvs[1]);

  return result;
}

}  // namespace device
