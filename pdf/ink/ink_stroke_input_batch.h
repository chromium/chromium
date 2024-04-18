// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_STROKE_INPUT_BATCH_H_
#define PDF_INK_INK_STROKE_INPUT_BATCH_H_

#include <memory>
#include <vector>

#include "pdf/ink/ink_stroke_input.h"

namespace chrome_pdf {

class InkStrokeInputBatch {
 public:
  static std::unique_ptr<InkStrokeInputBatch> Create(
      const std::vector<InkStrokeInput>& inputs);

  ~InkStrokeInputBatch() = default;

 protected:
  InkStrokeInputBatch() = default;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_STROKE_INPUT_BATCH_H_
