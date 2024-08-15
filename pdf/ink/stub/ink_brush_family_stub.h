// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_BRUSH_FAMILY_STUB_H_
#define PDF_INK_STUB_INK_BRUSH_FAMILY_STUB_H_

#include "pdf/ink/ink_brush_family.h"
#include "pdf/ink/ink_brush_tip.h"

namespace chrome_pdf {

class InkBrushFamilyStub : public InkBrushFamily {
 public:
  explicit InkBrushFamilyStub(InkBrushTip tip);
  InkBrushFamilyStub(const InkBrushFamilyStub&) = delete;
  InkBrushFamilyStub& operator=(const InkBrushFamilyStub&) = delete;
  ~InkBrushFamilyStub() override;

  // InkBrushFamily:
  float GetCornerRoundingForTesting() const override;
  float GetOpacityForTesting() const override;

 private:
  const float corner_rounding_;
  const float opacity_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_BRUSH_FAMILY_STUB_H_
