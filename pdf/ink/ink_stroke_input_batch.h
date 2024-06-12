// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_STROKE_INPUT_BATCH_H_
#define PDF_INK_INK_STROKE_INPUT_BATCH_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "pdf/ink/ink_stroke_input.h"

namespace chrome_pdf {

class InkStrokeInputBatch {
 public:
  static std::unique_ptr<InkStrokeInputBatch> Create(
      const std::vector<InkStrokeInput>& inputs);

  virtual ~InkStrokeInputBatch() = default;

  virtual size_t Size() const = 0;

  virtual InkStrokeInput Get(size_t i) const = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_STROKE_INPUT_BATCH_H_
