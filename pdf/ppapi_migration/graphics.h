// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_GRAPHICS_H_
#define PDF_PPAPI_MIGRATION_GRAPHICS_H_

#include "base/memory/raw_ptr.h"

class SkBitmap;
class SkSurface;

namespace gfx {
class Point;
class Rect;
class Vector2d;
class Vector2dF;
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

  // Sets the output scale factor. Must be greater than 0.
  virtual void SetScale(float scale) = 0;

  // Sets the output layer transform.
  virtual void SetLayerTransform(float scale,
                                 const gfx::Point& origin,
                                 const gfx::Vector2d& translate) = 0;

 protected:
  Graphics() = default;
};

// A Skia graphics device.
class SkiaGraphics final : public Graphics {
 public:
  // A client interface that needs to be registered when SkiaGraphics is
  // created.
  class Client {
   public:
    virtual ~Client() = default;

    // Updates the client with the latest output scale.
    virtual void UpdateScale(float scale) = 0;

    // Updates the client with the latest output layer transform.
    virtual void UpdateLayerTransform(float scale,
                                      const gfx::Vector2dF& translate) = 0;
  };

  // `client` and `surface` must outlive this object.
  SkiaGraphics(Client* client, SkSurface* surface);
  ~SkiaGraphics() override;

  void PaintImage(const SkBitmap& image, const gfx::Rect& src_rect) override;

  void Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) override;
  void SetScale(float scale) override;
  void SetLayerTransform(float scale,
                         const gfx::Point& origin,
                         const gfx::Vector2d& translate) override;

 private:
  // Unowned pointer. The client is required to outlive this object.
  raw_ptr<Client> client_;

  // Unowned pointer. The surface is required to outlive this object.
  raw_ptr<SkSurface> surface_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_GRAPHICS_H_
