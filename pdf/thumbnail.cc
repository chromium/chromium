// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/thumbnail.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/ranges.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr float kMinDevicePixelRatio = 0.25;
constexpr float kMaxDevicePixelRatio = 2;

}  // namespace

Thumbnail::Thumbnail() = default;

Thumbnail::Thumbnail(const gfx::Size& page_size, float device_pixel_ratio) {
  DCHECK_GE(device_pixel_ratio, kMinDevicePixelRatio);
  DCHECK_LE(device_pixel_ratio, kMaxDevicePixelRatio);
  device_pixel_ratio_ = base::ClampToRange(
      device_pixel_ratio, kMinDevicePixelRatio, kMaxDevicePixelRatio);

  // TODO(dhoss): Add conversion from page size to thumbnail size.
  const gfx::Size thumbnail_size_device_pixels = page_size;

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

Thumbnail::Thumbnail(Thumbnail&& other) noexcept = default;

Thumbnail& Thumbnail::operator=(Thumbnail&& other) noexcept = default;

Thumbnail::~Thumbnail() = default;

}  // namespace chrome_pdf
