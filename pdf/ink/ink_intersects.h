// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_INTERSECTS_H_
#define PDF_INK_INK_INTERSECTS_H_

namespace chrome_pdf {

class InkModeledShapeView;
struct InkAffineTransform;
struct InkRect;

bool InkIntersectsRectWithShape(const InkRect& rect,
                                const InkModeledShapeView& shape,
                                const InkAffineTransform& transform);

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_INTERSECTS_H_
