// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_GRAPHICS_H_
#define PDF_PPAPI_MIGRATION_GRAPHICS_H_

#include "base/memory/raw_ptr.h"

class SkBitmap;
class SkSurface;

namespace gfx {
class Rect;
class Vector2d;
}  // namespace gfx

namespace chrome_pdf {

// Abstraction for a Pepper or Skia graphics device.
class Graphics {
 public:
  virtual ~Graphics() = default;

  // Paints the `src_rect` region of `image` to the graphics device. The image
  // must be compatible with the concrete `Graphics` implementation.
  virtual void PaintImage(const SkBitmap& image, const gfx::Rect& src_rect) = 0;

  // Shifts the `clip` region of the graphics device by `amount`.
  virtual void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) = 0;

 protected:
  Graphics() = default;
};

// A Skia graphics device.
class SkiaGraphics final : public Graphics {
 public:
  // `surface` must outlive this object.
  explicit SkiaGraphics(SkSurface* surface);
  ~SkiaGraphics() override;

  void PaintImage(const SkBitmap& image, const gfx::Rect& src_rect) override;

  void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) override;

 private:
  // Unowned pointer. The surface is required to outlive this object.
  raw_ptr<SkSurface> surface_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_GRAPHICS_H_
