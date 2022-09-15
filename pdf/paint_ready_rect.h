// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAINT_READY_RECT_H_
#define PDF_PAINT_READY_RECT_H_

#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

class SkImage;

namespace chrome_pdf {

// Stores information about a rectangle that has finished painting. The
// `PaintManager` will paint it only when everything else on the screen is also
// ready.
class PaintReadyRect {
 public:
  PaintReadyRect(const gfx::Rect& rect,
                 sk_sp<SkImage> image,
                 bool flush_now = false);

  PaintReadyRect(const PaintReadyRect& other);
  PaintReadyRect& operator=(const PaintReadyRect& other);
  ~PaintReadyRect();

  const gfx::Rect& rect() const { return rect_; }
  void set_rect(const gfx::Rect& rect) { rect_ = rect; }

  const SkImage& image() const { return *image_; }

  // Whether to flush to screen immediately; otherwise, when the rest of the
  // plugin viewport is ready.
  bool flush_now() const { return flush_now_; }

 private:
  gfx::Rect rect_;
  sk_sp<SkImage> image_;
  bool flush_now_;
};

}  // namespace chrome_pdf

#endif  // PDF_PAINT_READY_RECT_H_
