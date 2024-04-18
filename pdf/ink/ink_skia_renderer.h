// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_SKIA_RENDERER_H_
#define PDF_INK_INK_SKIA_RENDERER_H_

#include <memory>

class GrDirectContext;
class SkCanvas;

namespace chrome_pdf {

class InkInProgressStroke;
class InkStroke;

class InkSkiaRenderer {
 public:
  // NOTE: This is the equivalent to the following 3x3 matrix:
  //
  //  a  b  c
  //  d  e  f
  //  0  0  1
  //
  // Thus the identity matrix is {1, 0, 0, 0, 1, 0}, and not {1, 0, 0, 1, 0, 0}.
  struct AffineTransform {
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
  };

  static std::unique_ptr<InkSkiaRenderer> Create();

  InkSkiaRenderer(const InkSkiaRenderer&) = delete;
  InkSkiaRenderer& operator=(const InkSkiaRenderer&) = delete;
  virtual ~InkSkiaRenderer() = default;

  // TODO(thestig): Remove `context` parameter.
  virtual bool Draw(GrDirectContext* context,
                    const InkInProgressStroke& stroke,
                    const AffineTransform& object_to_canvas,
                    SkCanvas& canvas) = 0;
  virtual bool Draw(GrDirectContext* context,
                    const InkStroke& stroke,
                    const AffineTransform& object_to_canvas,
                    SkCanvas& canvas) = 0;

 protected:
  InkSkiaRenderer() = default;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_SKIA_RENDERER_H_
