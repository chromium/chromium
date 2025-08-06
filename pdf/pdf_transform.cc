// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_transform.h"

#include <algorithm>
#include <utility>

#include "base/notreached.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

// Return the default size letter size (8.5" X 11") clip box. This just follows
// the PDFium way of handling these corner cases. PDFium always considers
// US-Letter as the default page size.
PdfRectangle GetDefaultClipBox(bool rotated) {
  constexpr int kDpi = 72;
  constexpr float kPaperWidth = 8.5 * kDpi;
  constexpr float kPaperHeight = 11 * kDpi;
  return PdfRectangle(/*left=*/0, /*bottom=*/0,
                      /*right=*/rotated ? kPaperHeight : kPaperWidth,
                      /*top=*/rotated ? kPaperWidth : kPaperHeight);
}

}  // namespace

void PdfRectangle::Normalize() {
  if (top_ < bottom_) {
    std::swap(top_, bottom_);
  }
  if (right_ < left_) {
    std::swap(right_, left_);
  }
}

void PdfRectangle::Scale(float scale_factor) {
  left_ *= scale_factor;
  bottom_ *= scale_factor;
  right_ *= scale_factor;
  top_ *= scale_factor;
}

void PdfRectangle::Intersect(const PdfRectangle& rect) {
  left_ = std::max(left_, rect.left_);
  bottom_ = std::max(bottom_, rect.bottom_);
  right_ = std::min(right_, rect.right_);
  top_ = std::min(top_, rect.top_);
}

float CalculateScaleFactor(const gfx::Rect& content_rect,
                           const gfx::SizeF& src_size,
                           bool rotated) {
  if (src_size.IsEmpty())
    return 1.0f;

  float actual_source_page_width =
      rotated ? src_size.height() : src_size.width();
  float actual_source_page_height =
      rotated ? src_size.width() : src_size.height();
  float ratio_x = content_rect.width() / actual_source_page_width;
  float ratio_y = content_rect.height() / actual_source_page_height;
  return std::min(ratio_x, ratio_y);
}

void CalculateMediaBoxAndCropBox(bool rotated,
                                 bool has_media_box,
                                 bool has_crop_box,
                                 PdfRectangle* media_box,
                                 PdfRectangle* crop_box) {
  if (has_media_box)
    media_box->Normalize();
  if (has_crop_box)
    crop_box->Normalize();

  if (!has_media_box && !has_crop_box) {
    *crop_box = GetDefaultClipBox(rotated);
    *media_box = *crop_box;
  } else if (has_crop_box && !has_media_box) {
    *media_box = *crop_box;
  } else if (has_media_box && !has_crop_box) {
    *crop_box = *media_box;
  }
}

PdfRectangle CalculateClipBoxBoundary(const PdfRectangle& media_box,
                                      const PdfRectangle& crop_box) {
  // Clip `media_box` to the size of `crop_box`, but ignore `crop_box` if it is
  // bigger than `media_box`.
  PdfRectangle clip_box = crop_box;
  clip_box.Intersect(media_box);
  return clip_box;
}

gfx::Vector2dF CalculateScaledClipBoxOffset(
    const gfx::Rect& content_rect,
    const PdfRectangle& source_clip_box) {
  // Center the intended clip region to real clip region.
  return gfx::Vector2dF((content_rect.width() - source_clip_box.width()) / 2 +
                            content_rect.x() - source_clip_box.left(),
                        (content_rect.height() - source_clip_box.height()) / 2 +
                            content_rect.y() - source_clip_box.bottom());
}

gfx::Vector2dF CalculateNonScaledClipBoxOffset(
    int rotation,
    int page_width,
    int page_height,
    const PdfRectangle& source_clip_box) {
  // Align the intended clip region to left-top corner of real clip region.
  switch (rotation) {
    case 0:
      return gfx::Vector2dF(-1 * source_clip_box.left(),
                            page_height - source_clip_box.top());
    case 1:
      return gfx::Vector2dF(0, -1 * source_clip_box.bottom());
    case 2:
      return gfx::Vector2dF(page_width - source_clip_box.right(), 0);
    case 3:
      return gfx::Vector2dF(page_height - source_clip_box.right(),
                            page_width - source_clip_box.top());
    default:
      NOTREACHED();
  }
}

}  // namespace chrome_pdf
