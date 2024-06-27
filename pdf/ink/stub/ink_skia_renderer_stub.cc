// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_skia_renderer_stub.h"

#include <memory>

namespace chrome_pdf {

// static
std::unique_ptr<InkSkiaRenderer> InkSkiaRenderer::Create() {
  return std::make_unique<InkSkiaRendererStub>();
}

bool InkSkiaRendererStub::Draw(const InkInProgressStroke& stroke,
                               const InkAffineTransform& object_to_canvas,
                               SkCanvas& canvas) {
  return true;
}

bool InkSkiaRendererStub::Draw(const InkStroke& stroke,
                               const InkAffineTransform& object_to_canvas,
                               SkCanvas& canvas) {
  return true;
}

}  // namespace chrome_pdf
