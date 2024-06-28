// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_intersects.h"

namespace chrome_pdf {

bool InkIntersectsRectWithShape(const InkRect& rect,
                                const InkModeledShapeView& shape,
                                const InkAffineTransform& transform) {
  return true;
}

}  // namespace chrome_pdf
