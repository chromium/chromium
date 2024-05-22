// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_intersects.h"

namespace chrome_pdf {

bool InkIntersectsPointWithShape(float point_x,
                                 float point_y,
                                 const InkModeledShape& shape,
                                 const InkAffineTransform& transform) {
  return false;
}

}  // namespace chrome_pdf
