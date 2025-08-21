// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_RECT_H_
#define PDF_PDF_RECT_H_

#include <stddef.h>

#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

// Represents PDF rectangles:
// - The origin is at the bottom-left.
// - All units are in points.
//
// Can be easily used with PDFium APIs that take either FS_RECTF, or 4 float
// pointers.
class PdfRect {
 public:
  constexpr PdfRect() : PdfRect(0, 0, 0, 0) {}
  // PdfRect and gfx::RectF have top/bottom flipped.
  constexpr explicit PdfRect(const gfx::RectF& rect)
      : left_(rect.x()),
        top_(rect.bottom()),
        right_(rect.right()),
        bottom_(rect.y()) {}
  constexpr PdfRect(float left, float bottom, float right, float top)
      : left_(left), top_(top), right_(right), bottom_(bottom) {}
  constexpr ~PdfRect() = default;

  float left() const { return left_; }
  float bottom() const { return bottom_; }
  float right() const { return right_; }
  float top() const { return top_; }

  // These return pointers so they can be directly passed into PDFium's public
  // APIs, which are written in C.
  float* writable_left() { return &left_; }
  float* writable_bottom() { return &bottom_; }
  float* writable_right() { return &right_; }
  float* writable_top() { return &top_; }

  float width() const { return right_ - left_; }
  float height() const { return top_ - bottom_; }

  gfx::RectF AsGfxRectF() const;

  void Offset(float horizontal, float vertical);

  bool IsEmpty() const { return !width() || !height(); }

  // When a PdfRect has top < bottom, or right < left, the values should be
  // swapped.
  void Normalize();

  void Scale(float scale_factor);

  void Intersect(const PdfRect& rect);

  void Union(const PdfRect& rect);

  friend constexpr bool operator==(const PdfRect&, const PdfRect&) = default;

  // Exposes offsetof() values for the private variables.
  static constexpr size_t offsetof_left() { return offsetof(PdfRect, left_); }
  static constexpr size_t offsetof_bottom() {
    return offsetof(PdfRect, bottom_);
  }
  static constexpr size_t offsetof_right() { return offsetof(PdfRect, right_); }
  static constexpr size_t offsetof_top() { return offsetof(PdfRect, top_); }

 private:
  // Do not change the ordering here, or add new member variables. This is
  // arranged to be the same order as PDFium's FS_RECTF.
  float left_;
  float top_;
  float right_;
  float bottom_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_RECT_H_
