// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_GRAPHICS_H_
#define PDF_PPAPI_MIGRATION_GRAPHICS_H_

#include "pdf/ppapi_migration/callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Point;
class Rect;
class Vector2d;
}  // namespace gfx

namespace pp {
class InstanceHandle;
}  // namespace pp

namespace chrome_pdf {

class Image;

// Abstraction for a Pepper or Skia graphics device.
// TODO(crbug.com/1099020): Implement the Skia graphics device.
class Graphics {
 public:
  virtual ~Graphics() = default;

  const gfx::Size& size() const { return size_; }

  // Flushes pending operations, invoking the callback on completion. Returns
  // `true` if the callback is still pending.
  virtual bool Flush(ResultCallback callback) = 0;

  // Paints the |src_rect| region of |image| to the graphics device. The image
  // must be compatible with the concrete `Graphics` implementation.
  virtual void PaintImage(const Image& image, const gfx::Rect& src_rect) = 0;

  // Shifts the |clip| region of the graphics device by |amount|.
  virtual void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) = 0;

  // Sets the output scale factor. Must be greater than 0.
  virtual void SetScale(float scale) = 0;

  // Sets the output layer transform.
  virtual void SetLayerTransform(float scale,
                                 const gfx::Point& origin,
                                 const gfx::Vector2d& translate) = 0;

 protected:
  explicit Graphics(const gfx::Size& size);

 private:
  gfx::Size size_;
};

// A Pepper graphics device.
class PepperGraphics final : public Graphics {
 public:
  PepperGraphics(const pp::InstanceHandle& instance, const gfx::Size& size);
  ~PepperGraphics() override;

  bool Flush(ResultCallback callback) override;

  void PaintImage(const Image& image, const gfx::Rect& src_rect) override;

  void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) override;
  void SetScale(float scale) override;
  void SetLayerTransform(float scale,
                         const gfx::Point& origin,
                         const gfx::Vector2d& translate) override;

  // Gets the underlying pp::Graphics2D.
  pp::Graphics2D& pepper_graphics() { return pepper_graphics_; }

 private:
  pp::Graphics2D pepper_graphics_;
};

// A Skia graphics device.
class SkiaGraphics final : public Graphics {
 public:
  static std::unique_ptr<SkiaGraphics> Create(const gfx::Size& size);

  ~SkiaGraphics() override;

  bool Flush(ResultCallback callback) override;

  void PaintImage(const Image& image, const gfx::Rect& src_rect) override;

  void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) override;
  void SetScale(float scale) override;
  void SetLayerTransform(float scale,
                         const gfx::Point& origin,
                         const gfx::Vector2d& translate) override;

  sk_sp<SkImage> CreateSnapshot();

 private:
  explicit SkiaGraphics(const gfx::Size& size);

  sk_sp<SkSurface> skia_graphics_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_GRAPHICS_H_
