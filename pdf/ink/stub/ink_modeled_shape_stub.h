// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_MODELED_SHAPE_STUB_H_
#define PDF_INK_STUB_INK_MODELED_SHAPE_STUB_H_

#include "pdf/ink/ink_modeled_shape.h"

namespace chrome_pdf {

class InkModeledShapeStub : public InkModeledShape {
 public:
  InkModeledShapeStub();
  ~InkModeledShapeStub() override;

  // InkModeledShape:
  uint32_t RenderGroupCount() const override;
  std::vector<Outline> GetOutlines(uint32_t group_index) const override;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_MODELED_SHAPE_STUB_H_
