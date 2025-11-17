// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_transform.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "pdf/pdf_rect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

// Return the default size letter size (8.5" X 11") clip box. This just follows
// the PDFium way of handling these corner cases. PDFium always considers
// US-Letter as the default page size.
PdfRect GetDefaultClipBox(bool rotated) {
  constexpr int kDpi = 72;
  constexpr float kPaperWidth = 8.5 * kDpi;
  constexpr float kPaperHeight = 11 * kDpi;
  return PdfRect(/*left=*/0, /*bottom=*/0,
                 /*right=*/rotated ? kPaperHeight : kPaperWidth,
                 /*top=*/rotated ? kPaperWidth : kPaperHeight);
}

}  // namespace

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
                                 PdfRect* media_box,
                                 PdfRect* crop_box) {
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

PdfRect CalculateClipBoxBoundary(const PdfRect& media_box,
                                 const PdfRect& crop_box) {
  // Clip `media_box` to the size of `crop_box`, but ignore `crop_box` if it is
  // bigger than `media_box`.
  PdfRect clip_box = crop_box;
  clip_box.Intersect(media_box);
  return clip_box;
}

gfx::Vector2dF CalculateScaledClipBoxOffset(const gfx::Rect& content_rect,
                                            const PdfRect& source_clip_box) {
  // Center the intended clip region to real clip region.
  return gfx::Vector2dF((content_rect.width() - source_clip_box.width()) / 2 +
                            content_rect.x() - source_clip_box.left(),
                        (content_rect.height() - source_clip_box.height()) / 2 +
                            content_rect.y() - source_clip_box.bottom());
}

gfx::Vector2dF CalculateCenterClipBoxOffset(int rotation,
                                            int page_width,
                                            int page_height,
                                            const PdfRect& source_clip_box) {
  if ((rotation % 2) == 1) {
    std::swap(page_width, page_height);
  }

  // Center the source clip box only if it is smaller than the page size.
  CHECK_GE(page_width, source_clip_box.width());
  CHECK_GE(page_height, source_clip_box.height());

  return gfx::Vector2dF((page_width - source_clip_box.width()) / 2,
                        (page_height - source_clip_box.height()) / 2);
}

gfx::Vector2dF CalculateNonScaledClipBoxOffset(int rotation,
                                               int page_width,
                                               int page_height,
                                               const PdfRect& source_clip_box) {
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
