// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_TRANSFORM_H_
#define PDF_PDF_TRANSFORM_H_

#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class Rect;
class SizeF;
}  // namespace gfx

namespace chrome_pdf {

// All the code here works in the PDF coordinate space. The origin is at the
// bottom-left, and all units are in points.

// Represents PDF rectangles with the properties stated above.
// Can be easily used with PDFium's bounding box functions.
class PdfRectangle {
 public:
  constexpr PdfRectangle() : PdfRectangle(0, 0, 0, 0) {}
  constexpr PdfRectangle(float left, float bottom, float right, float top)
      : left_(left), bottom_(bottom), right_(right), top_(top) {}
  constexpr ~PdfRectangle() = default;

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

  // When a PdfRectangle has top < bottom, or right < left, the values should be
  // swapped.
  void Normalize();

  void Scale(float scale_factor);

  void Intersect(const PdfRectangle& rect);

 private:
  float left_;
  float bottom_;
  float right_;
  float top_;
};

// Calculate the scale factor between `content_rect` and a page of `src_size`.
//
// `content_rect` specifies the printable area of the destination page.
// `src_size` specifies the source page size.
// `rotated` True if source page is rotated 90 degree or 270 degree.
float CalculateScaleFactor(const gfx::Rect& content_rect,
                           const gfx::SizeF& src_size,
                           bool rotated);

// Set the media box and/or crop box as needed. If both boxes are there, then
// nothing needs to be done. If one box is missing, then fill it with the value
// from the other box. If both boxes are missing, then they both get the default
// value from GetDefaultClipBox(), based on `rotated`.
void CalculateMediaBoxAndCropBox(bool rotated,
                                 bool has_media_box,
                                 bool has_crop_box,
                                 PdfRectangle* media_box,
                                 PdfRectangle* crop_box);

// Compute source clip box boundaries based on the crop box / media box of
// source page and scale factor.
// Returns the computed source clip box values.
//
// `media_box` The PDF's media box.
// `crop_box` The PDF's crop box.
PdfRectangle CalculateClipBoxBoundary(const PdfRectangle& media_box,
                                      const PdfRectangle& crop_box);

// Calculate the clip box translation offset for a page that does need to be
// scaled.
//
// `content_rect` specifies the printable area of the destination page.
// `source_clip_box` specifies the source clip box positions, relative to the
// origin.
// Returns the final translation offsets for the source clip box, relative to
// the origin.
gfx::Vector2dF CalculateScaledClipBoxOffset(
    const gfx::Rect& content_rect,
    const PdfRectangle& source_clip_box);

// Calculate the clip box offset for a page that does not need to be scaled.
//
// `rotation` specifies the source page rotation values which are N / 90
// degrees.
// `page_width` specifies the screen destination page width.
// `page_height` specifies the screen destination page height.
// `source_clip_box` specifies the source clip box positions, relative to the
// origin.
// Returns the final translation offsets for the source clip box, relative to
// the origin.
gfx::Vector2dF CalculateNonScaledClipBoxOffset(
    int rotation,
    int page_width,
    int page_height,
    const PdfRectangle& source_clip_box);

}  // namespace chrome_pdf

#endif  // PDF_PDF_TRANSFORM_H_
