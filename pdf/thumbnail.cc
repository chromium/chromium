// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/thumbnail.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr float kMinDevicePixelRatio = 0.25;
constexpr float kMaxDevicePixelRatio = 2;

constexpr int kImageColorChannels = 4;

// TODO(crbug.com/702993): Reevaluate the thumbnail size cap when the PDF
// component migrates off of PPAPI.
// The maximum thumbnail area is essentially arbitrary, but the value was chosen
// considering the fact that when sending array buffers through PPAPI, if the
// size of the data is over 256KiB, it gets sent using shared memory instead of
// IPC. Thumbnail sizes are capped at 255KiB to avoid the 256KiB threshold for
// images and their metadata, such as size.
constexpr int kMaxThumbnailPixels = 255 * 1024 / kImageColorChannels;

// Maximum CSS dimensions are set to match UX specifications.
// These constants should be kept in sync with `PORTRAIT_WIDTH` and
// `LANDSCAPE_WIDTH` in
// chrome/browser/resources/pdf/elements/viewer-thumbnail.js.
constexpr int kMaxWidthPortraitPx = 108;
constexpr int kMaxWidthLandscapePx = 140;

// PDF page size limits in default user space units, as defined by PDF 1.7 Annex
// C.2, "Architectural limits".
constexpr int kPdfPageMinDimension = 3;
constexpr int kPdfPageMaxDimension = 14400;
constexpr int kPdfMaxAspectRatio = kPdfPageMaxDimension / kPdfPageMinDimension;

// Limit the proportions within PDF limits to handle pathological PDF pages.
gfx::Size LimitAspectRatio(gfx::Size page_size) {
  // Bump up any lengths of 0 to 1.
  page_size.SetToMax(gfx::Size(1, 1));

  if (page_size.height() / page_size.width() > kPdfMaxAspectRatio)
    return gfx::Size(kPdfPageMinDimension, kPdfPageMaxDimension);
  if (page_size.width() / page_size.height() > kPdfMaxAspectRatio)
    return gfx::Size(kPdfPageMaxDimension, kPdfPageMinDimension);

  return page_size;
}

// Calculate the size of a thumbnail image in device pixels using |page_size| in
// any units and |device_pixel_ratio|.
gfx::Size CalculateBestFitSize(const gfx::Size& page_size,
                               float device_pixel_ratio) {
  gfx::Size safe_page_size = LimitAspectRatio(page_size);

  // Return the larger of the unrotated and rotated sizes to over-sample the PDF
  // page so that the thumbnail looks good in different orientations.
  float scale_portrait =
      static_cast<float>(kMaxWidthPortraitPx) /
      std::min(safe_page_size.width(), safe_page_size.height());
  float scale_landscape =
      static_cast<float>(kMaxWidthLandscapePx) /
      std::max(safe_page_size.width(), safe_page_size.height());
  float scale = std::max(scale_portrait, scale_landscape) * device_pixel_ratio;

  // Using gfx::ScaleToFlooredSize() is fine because `scale` will not yield an
  // empty size unless `device_pixel_ratio` is very small (close to 0).
  // However, `device_pixel_ratio` support is limited to between 0.25 and 2.
  gfx::Size scaled_size = gfx::ScaleToFlooredSize(safe_page_size, scale);
  if (scaled_size.GetCheckedArea().ValueOrDefault(kMaxThumbnailPixels + 1) >
      kMaxThumbnailPixels) {
    // Recalculate `scale` to accommodate pixel size limit such that:
    // (scale * safe_page_size.width()) * (scale * safe_page_size.height()) ==
    //     kMaxThumbnailPixels;
    scale = std::sqrt(static_cast<float>(kMaxThumbnailPixels) /
                      safe_page_size.width() / safe_page_size.height());
    return gfx::ScaleToFlooredSize(safe_page_size, scale);
  }

  return scaled_size;
}

}  // namespace

Thumbnail::Thumbnail() = default;

Thumbnail::Thumbnail(const gfx::Size& page_size, float device_pixel_ratio) {
  DCHECK_GE(device_pixel_ratio, kMinDevicePixelRatio);
  DCHECK_LE(device_pixel_ratio, kMaxDevicePixelRatio);
  device_pixel_ratio_ = base::ClampToRange(
      device_pixel_ratio, kMinDevicePixelRatio, kMaxDevicePixelRatio);

  const gfx::Size thumbnail_size_device_pixels =
      CalculateBestFitSize(page_size, device_pixel_ratio_);

  // Note that <canvas> can only hold data in RGBA format. It is the
  // responsibility of the thumbnail's renderer to fill `bitmap_` with RGBA
  // data.
  const SkImageInfo info =
      SkImageInfo::Make(thumbnail_size_device_pixels.width(),
                        thumbnail_size_device_pixels.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  bool success = bitmap_.tryAllocPixels(info, info.minRowBytes());
  DCHECK(success);
}

Thumbnail::Thumbnail(Thumbnail&& other) = default;

Thumbnail& Thumbnail::operator=(Thumbnail&& other) = default;

Thumbnail::~Thumbnail() = default;

}  // namespace chrome_pdf
