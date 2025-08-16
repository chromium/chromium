// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_RECT_H_
#define PDF_PDF_RECT_H_

namespace chrome_pdf {

// Represents PDF rectangles:
// - The origin is at the bottom-left.
// - All units are in points.
//
// Can be easily used with PDFium's bounding box functions.
class PdfRect {
 public:
  constexpr PdfRect() : PdfRect(0, 0, 0, 0) {}
  constexpr PdfRect(float left, float bottom, float right, float top)
      : left_(left), bottom_(bottom), right_(right), top_(top) {}
  constexpr ~PdfRect() = default;

  float left() const { return left_; }
  float bottom() const { return bottom_; }
  float right() const { return right_; }
  float top() const { return top_; }

  // These return pointers so they can be directly passed into PDFium's public
  // API, which is written in C.
  float* writable_left() { return &left_; }
  float* writable_bottom() { return &bottom_; }
  float* writable_right() { return &right_; }
  float* writable_top() { return &top_; }

  float width() const { return right_ - left_; }
  float height() const { return top_ - bottom_; }

  bool IsEmpty() const { return !width() || !height(); }

  // When a PdfRect has top < bottom, or right < left, the values should be
  // swapped.
  void Normalize();

  void Scale(float scale_factor);

  void Intersect(const PdfRect& rect);

 private:
  float left_;
  float bottom_;
  float right_;
  float top_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_RECT_H_
