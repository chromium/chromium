// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_transform.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

// When a PdfRectangle has top < bottom, or right < left, the values should be
// swapped.
void SwapPdfRectangleValuesIfNeeded(PdfRectangle* rect) {
  if (rect->top < rect->bottom)
    std::swap(rect->top, rect->bottom);
  if (rect->right < rect->left)
    std::swap(rect->right, rect->left);
}

}  // namespace

double CalculateScaleFactor(const gfx::Rect& content_rect,
                            double src_width,
                            double src_height,
                            bool rotated) {
  if (src_width == 0 || src_height == 0)
    return 1.0;

  double actual_source_page_width = rotated ? src_height : src_width;
  double actual_source_page_height = rotated ? src_width : src_height;
  double ratio_x =
      static_cast<double>(content_rect.width()) / actual_source_page_width;
  double ratio_y =
      static_cast<double>(content_rect.height()) / actual_source_page_height;
  return std::min(ratio_x, ratio_y);
}

void SetDefaultClipBox(bool rotated, PdfRectangle* clip_box) {
  constexpr int kDpi = 72;
  constexpr float kPaperWidth = 8.5 * kDpi;
  constexpr float kPaperHeight = 11 * kDpi;
  clip_box->left = 0;
  clip_box->bottom = 0;
  clip_box->right = rotated ? kPaperHeight : kPaperWidth;
  clip_box->top = rotated ? kPaperWidth : kPaperHeight;
}

void CalculateMediaBoxAndCropBox(bool rotated,
                                 bool has_media_box,
                                 bool has_crop_box,
                                 PdfRectangle* media_box,
                                 PdfRectangle* crop_box) {
  if (has_media_box)
    SwapPdfRectangleValuesIfNeeded(media_box);
  if (has_crop_box)
    SwapPdfRectangleValuesIfNeeded(crop_box);

  if (!has_media_box && !has_crop_box) {
    SetDefaultClipBox(rotated, crop_box);
    SetDefaultClipBox(rotated, media_box);
  } else if (has_crop_box && !has_media_box) {
    *media_box = *crop_box;
  } else if (has_media_box && !has_crop_box) {
    *crop_box = *media_box;
  }
}

PdfRectangle CalculateClipBoxBoundary(const PdfRectangle& media_box,
                                      const PdfRectangle& crop_box) {
  PdfRectangle clip_box;

  // Clip |media_box| to the size of |crop_box|, but ignore |crop_box| if it is
  // bigger than |media_box|.
  clip_box.left = std::max(crop_box.left, media_box.left);
  clip_box.bottom = std::max(crop_box.bottom, media_box.bottom);
  clip_box.right = std::min(crop_box.right, media_box.right);
  clip_box.top = std::min(crop_box.top, media_box.top);
  return clip_box;
}

void ScalePdfRectangle(double scale_factor, PdfRectangle* rect) {
  rect->left *= scale_factor;
  rect->bottom *= scale_factor;
  rect->right *= scale_factor;
  rect->top *= scale_factor;
}

void CalculateScaledClipBoxOffset(const gfx::Rect& content_rect,
                                  const PdfRectangle& source_clip_box,
                                  double* offset_x,
                                  double* offset_y) {
  const float clip_box_width = source_clip_box.right - source_clip_box.left;
  const float clip_box_height = source_clip_box.top - source_clip_box.bottom;

  // Center the intended clip region to real clip region.
  *offset_x = (content_rect.width() - clip_box_width) / 2 + content_rect.x() -
              source_clip_box.left;
  *offset_y = (content_rect.height() - clip_box_height) / 2 + content_rect.y() -
              source_clip_box.bottom;
}

void CalculateNonScaledClipBoxOffset(int rotation,
                                     int page_width,
                                     int page_height,
                                     const PdfRectangle& source_clip_box,
                                     double* offset_x,
                                     double* offset_y) {
  // Align the intended clip region to left-top corner of real clip region.
  switch (rotation) {
    case 0:
      *offset_x = -1 * source_clip_box.left;
      *offset_y = page_height - source_clip_box.top;
      break;
    case 1:
      *offset_x = 0;
      *offset_y = -1 * source_clip_box.bottom;
      break;
    case 2:
      *offset_x = page_width - source_clip_box.right;
      *offset_y = 0;
      break;
    case 3:
      *offset_x = page_height - source_clip_box.right;
      *offset_y = page_width - source_clip_box.top;
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace chrome_pdf
