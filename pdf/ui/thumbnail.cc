// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/thumbnail.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/values.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr float kMinDevicePixelRatio = 0.25;
constexpr float kMaxDevicePixelRatio = 2;

constexpr int kImageColorChannels = 4;

// TODO(crbug.com/40511452): Reevaluate the thumbnail size cap when the PDF
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
// chrome/browser/resources/pdf/elements/viewer-thumbnail.ts.
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

// Calculate the size of a thumbnail image in device pixels using `page_size` in
// any units and `device_pixel_ratio`.
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

int CalculateStride(int width) {
  base::CheckedNumeric<size_t> stride = kImageColorChannels;
  stride *= width;
  return stride.ValueOrDie<int>();
}

size_t CalculateImageDataSize(int stride, int height) {
  base::CheckedNumeric<int> size = stride;
  size *= height;
  return size.ValueOrDie<size_t>();
}

}  // namespace

Thumbnail::Thumbnail(const gfx::Size& page_size, float device_pixel_ratio)
    : device_pixel_ratio_(std::clamp(device_pixel_ratio,
                                     kMinDevicePixelRatio,
                                     kMaxDevicePixelRatio)),
      image_size_(CalculateBestFitSize(page_size, device_pixel_ratio_)),
      stride_(CalculateStride(image_size_.width())),
      image_data_(CalculateImageDataSize(stride(), image_size().height())) {
  DCHECK(!image_data_.empty());
}

Thumbnail::Thumbnail(Thumbnail&& other) noexcept = default;

Thumbnail& Thumbnail::operator=(Thumbnail&& other) noexcept = default;

Thumbnail::~Thumbnail() = default;

base::Value::BlobStorage& Thumbnail::GetImageData() {
  DCHECK(!image_data_.empty());
  return image_data_;
}

base::Value::BlobStorage Thumbnail::TakeData() {
  DCHECK(!image_data_.empty());
  return std::move(image_data_);
}

}  // namespace chrome_pdf
