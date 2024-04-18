// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_SKIA_RENDERER_STUB_H_
#define PDF_INK_STUB_INK_SKIA_RENDERER_STUB_H_

#include "pdf/ink/ink_skia_renderer.h"

namespace chrome_pdf {

class InkSkiaRendererStub : public InkSkiaRenderer {
 public:
  bool Draw(GrDirectContext* context,
            const InkInProgressStroke& stroke,
            const AffineTransform& object_to_canvas,
            SkCanvas& canvas) override;
  bool Draw(GrDirectContext* context,
            const InkStroke& stroke,
            const AffineTransform& object_to_canvas,
            SkCanvas& canvas) override;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_SKIA_RENDERER_STUB_H_
