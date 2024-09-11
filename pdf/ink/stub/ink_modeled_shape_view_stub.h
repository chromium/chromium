// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_MODELED_SHAPE_VIEW_STUB_H_
#define PDF_INK_STUB_INK_MODELED_SHAPE_VIEW_STUB_H_

#include "pdf/ink/ink_modeled_shape_view.h"

namespace chrome_pdf {

class InkModeledShapeViewStub : public InkModeledShapeView {
 public:
  InkModeledShapeViewStub();
  InkModeledShapeViewStub(const InkModeledShapeViewStub&) = delete;
  InkModeledShapeViewStub& operator=(const InkModeledShapeViewStub&) = delete;
  ~InkModeledShapeViewStub() override;

  // InkModeledShapeView:
  uint32_t RenderGroupCount() const override;
  uint32_t OutlineCount(uint32_t group_index) const override;
  std::vector<OutlinePositions> GetRenderGroupOutlinePositions(
      uint32_t group_index) const override;
  InkRect Bounds() const override;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_MODELED_SHAPE_VIEW_STUB_H_
