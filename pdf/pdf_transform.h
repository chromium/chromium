// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_TRANSFORM_H_
#define PDF_PDF_TRANSFORM_H_

namespace gfx {
class PointF;
class Rect;
class SizeF;
}  // namespace gfx

namespace chrome_pdf {

// All the code here works in the PDF coordinate space. The origin is at the
// bottom-left, and all units are in points.

// A rect struct for use with FPDF bounding box functions.
struct PdfRectangle {
  float left;
  float bottom;
  float right;
  float top;
};

// Calculate the scale factor between `content_rect` and a page of `src_size`.
//
// `content_rect` specifies the printable area of the destination page.
// `src_size` specifies the source page size.
// `rotated` True if source page is rotated 90 degree or 270 degree.
float CalculateScaleFactor(const gfx::Rect& content_rect,
                           const gfx::SizeF& src_size,
                           bool rotated);

// Make the default size to be letter size (8.5" X 11"). We are just following
// the PDFium way of handling these corner cases. PDFium always consider
// US-Letter as the default page size.
void SetDefaultClipBox(bool rotated, PdfRectangle* clip_box);

// Set the media box and/or crop box as needed. If both boxes are there, then
// nothing needs to be done. If one box is missing, then fill it with the value
// from the other box. If both boxes are missing, then they both get the default
// value from SetDefaultClipBox(), based on `rotated`.
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

// Scale `rect` by `scale_factor`.
void ScalePdfRectangle(float scale_factor, PdfRectangle* rect);

// Calculate the clip box translation offset for a page that does need to be
// scaled.
//
// `content_rect` specifies the printable area of the destination page.
// `source_clip_box` specifies the source clip box positions, relative to the
// origin.
// Returns the final translation offsets for the source clip box, relative to
// the origin.
gfx::PointF CalculateScaledClipBoxOffset(const gfx::Rect& content_rect,
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
gfx::PointF CalculateNonScaledClipBoxOffset(
    int rotation,
    int page_width,
    int page_height,
    const PdfRectangle& source_clip_box);

}  // namespace chrome_pdf

#endif  // PDF_PDF_TRANSFORM_H_
