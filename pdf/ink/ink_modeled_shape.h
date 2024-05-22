// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_MODELED_SHAPE_H_
#define PDF_INK_INK_MODELED_SHAPE_H_

#include <stdint.h>

#include <vector>

namespace chrome_pdf {

class InkModeledShape {
 public:
  struct VertexIndexPair {
    uint16_t mesh_index;
    uint16_t vertex_index;
  };
  using Outline = std::vector<VertexIndexPair>;

  virtual ~InkModeledShape() = default;

  virtual uint32_t RenderGroupCount() const = 0;
  virtual std::vector<Outline> GetOutlines(uint32_t group_index) const = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_MODELED_SHAPE_H_
