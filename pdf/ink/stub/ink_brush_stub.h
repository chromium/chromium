// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_BRUSH_STUB_H_
#define PDF_INK_STUB_INK_BRUSH_STUB_H_

#include <memory>

#include "pdf/ink/ink_brush.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

class InkBrushFamily;

class InkBrushStub : public InkBrush {
 public:
  InkBrushStub(std::unique_ptr<InkBrushFamily> family,
               SkColor color,
               float size,
               float epsilon);
  InkBrushStub(const InkBrushStub&) = delete;
  InkBrushStub& operator=(const InkBrushStub&) = delete;
  ~InkBrushStub() override;

  // InkBrush:
  float GetSize() const override;
  SkColor GetColor() const override;
  float GetOpacityForTesting() const override;

 private:
  std::unique_ptr<InkBrushFamily> family_;
  const SkColor color_;
  const float size_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_BRUSH_STUB_H_
