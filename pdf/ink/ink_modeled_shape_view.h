// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_MODELED_SHAPE_VIEW_H_
#define PDF_INK_INK_MODELED_SHAPE_VIEW_H_

#include <stdint.h>

#include <vector>

#include "pdf/ink/ink_point.h"
#include "pdf/ink/ink_rect.h"

namespace chrome_pdf {

class InkModeledShapeView {
 public:
  using OutlinePositions = std::vector<InkPoint>;

  virtual ~InkModeledShapeView() = default;

  virtual uint32_t RenderGroupCount() const = 0;

  // Gets the collection of all outline positions for the 0-based render group
  // index.
  virtual std::vector<OutlinePositions> GetRenderGroupOutlinePositions(
      uint32_t group_index) const = 0;

  // Note that the return type is simpler and more straight-forward than Ink's.
  virtual InkRect Bounds() const = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_MODELED_SHAPE_VIEW_H_
