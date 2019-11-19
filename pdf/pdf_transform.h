// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_TRANSFORM_H_
#define PDF_PDF_TRANSFORM_H_

namespace gfx {
class Rect;
}

namespace chrome_pdf {

// A rect struct for use with FPDF bounding box functions.
// With PDFs, origin is bottom-left.
struct PdfRectangle {
  float left;
  float bottom;
  float right;
  float top;
};

// Calculate the scale factor between |content_rect| and a page of size
// |src_width| x |src_height|.
//
// |content_rect| specifies the printable area of the destination page, with
// origin at left-bottom. Values are in points.
// |src_width| specifies the source page width in points.
// |src_height| specifies the source page height in points.
// |rotated| True if source page is rotated 90 degree or 270 degree.
double CalculateScaleFactor(const gfx::Rect& content_rect,
                            double src_width,
                            double src_height,
                            bool rotated);

// Make the default size to be letter size (8.5" X 11"). We are just following
// the PDFium way of handling these corner cases. PDFium always consider
// US-Letter as the default page size.
void SetDefaultClipBox(bool rotated, PdfRectangle* clip_box);

// Set the media box and/or crop box as needed. If both boxes are there, then
// nothing needs to be done. If one box is missing, then fill it with the value
// from the other box. If both boxes are missing, then they both get the default
// value from SetDefaultClipBox(), based on |rotated|.
void CalculateMediaBoxAndCropBox(bool rotated,
                                 bool has_media_box,
                                 bool has_crop_box,
                                 PdfRectangle* media_box,
                                 PdfRectangle* crop_box);

// Compute source clip box boundaries based on the crop box / media box of
// source page and scale factor.
// Returns the computed source clip box values.
//
// |media_box| The PDF's media box.
// |crop_box| The PDF's crop box.
PdfRectangle CalculateClipBoxBoundary(const PdfRectangle& media_box,
                                      const PdfRectangle& crop_box);

// Scale |rect| by |scale_factor|.
void ScalePdfRectangle(double scale_factor, PdfRectangle* rect);

// Calculate the clip box translation offset for a page that does need to be
// scaled. All parameters are in points.
//
// |content_rect| specifies the printable area of the destination page, with
// origin at left-bottom.
// |source_clip_box| specifies the source clip box positions, relative to
// origin at left-bottom.
// |offset_x| and |offset_y| will contain the final translation offsets for the
// source clip box, relative to origin at left-bottom.
void CalculateScaledClipBoxOffset(const gfx::Rect& content_rect,
                                  const PdfRectangle& source_clip_box,
                                  double* offset_x,
                                  double* offset_y);

// Calculate the clip box offset for a page that does not need to be scaled.
// All parameters are in points.
//
// |rotation| specifies the source page rotation values which are N / 90
// degrees.
// |page_width| specifies the screen destination page width.
// |page_height| specifies the screen destination page height.
// |source_clip_box| specifies the source clip box positions, relative to origin
// at left-bottom.
// |offset_x| and |offset_y| will contain the final translation offsets for the
// source clip box, relative to origin at left-bottom.
void CalculateNonScaledClipBoxOffset(int rotation,
                                     int page_width,
                                     int page_height,
                                     const PdfRectangle& source_clip_box,
                                     double* offset_x,
                                     double* offset_y);

}  // namespace chrome_pdf

#endif  // PDF_PDF_TRANSFORM_H_
