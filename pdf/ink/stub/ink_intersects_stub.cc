// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_intersects.h"

namespace chrome_pdf {

bool InkIntersectsRectWithShape(float rect_x,
                                float rect_y,
                                float rect_width,
                                float rect_height,
                                const InkModeledShape& shape,
                                const InkAffineTransform& transform) {
  return false;
}

}  // namespace chrome_pdf
