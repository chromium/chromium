// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/image_util.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/common/color_parser.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/codec/png_codec.h"

namespace extensions::image_util {

bool IsIconSufficientlyVisible(const SkBitmap& bitmap) {
  // TODO(crbug.com/40559794): Currently, we only consider if there are enough
  // visible pixels that it won't be difficult for the user to see. Future
  // revisions will consider the background color of the display context.

  // If the alpha value of any pixel is greater than kAlphaThreshold, the
  // pixmap is not transparent. These values will likely be adjusted, based
  // on stats and research into visibility thresholds.
  constexpr unsigned int kAlphaThreshold = 10;
  // The minimum "percent" of pixels that must be visible for the icon to be
  // considered OK.
  constexpr double kMinPercentVisiblePixels = 0.03;
  const int total_pixels = bitmap.height() * bitmap.width();
  // Pre-calculate the minimum number of visible pixels so we can exit early.
  // Since we expect most icons to be visible, this will perform better for
  // the common case.
  const int minimum_visible_pixels =
      std::max(kMinPercentVisiblePixels * total_pixels, 1.0);

  int visible_pixels = 0;
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      if (SkColorGetA(bitmap.getColor(x, y)) >= kAlphaThreshold) {
        if (++visible_pixels == minimum_visible_pixels) {
          return true;
        }
      }
    }
  }
  return false;
}

bool IsIconAtPathSufficientlyVisible(const base::FilePath& path) {
  SkBitmap icon;
  if (!LoadPngFromFile(path, &icon)) {
    return false;
  }
  return IsIconSufficientlyVisible(icon);
}

const SkColor kDefaultToolbarColor = SK_ColorWHITE;

struct ScopedUmaMicrosecondHistogramTimer {
  ScopedUmaMicrosecondHistogramTimer() : timer() {}

  ~ScopedUmaMicrosecondHistogramTimer() {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.IsRenderedIconSufficientlyVisibleTime", timer.Elapsed(),
        base::Microseconds(1), base::Seconds(5), 50);
  }

  const base::ElapsedTimer timer;
};

bool IsRenderedIconSufficientlyVisible(const SkBitmap& icon,
                                       SkColor background_color) {
  const ScopedUmaMicrosecondHistogramTimer timer;

  // If any of a pixel's RGB values is greater than this number, the pixel is
  // considered visible.
  constexpr unsigned int kThreshold = 7;
  // The minimum "percent" of pixels that must be visible for the icon to be
  // considered OK.
  constexpr double kMinPercentVisiblePixels = 0.03;
  const int total_pixels = icon.height() * icon.width();
  // Pre-calculate the minimum number of visible pixels so we can exit early.
  // Since we expect most icons to be visible, this will perform better for
  // the common case.
  const int minimum_visible_pixels =
      std::max(kMinPercentVisiblePixels * total_pixels, 1.0);

  // Draw the icon onto a canvas, then draw the background color onto the
  // resulting bitmap, using SkBlendMode::kDifference. Then, check the RGB
  // values against the threshold. Any pixel with a value greater than the
  // threshold is considered visible. If analysis fails, don't render the icon.
  SkBitmap bitmap;
  if (!RenderIconForVisibilityAnalysis(icon, background_color, &bitmap)) {
    return false;
  }

  int visible_pixels = 0;
  for (int x = 0; x < icon.width(); ++x) {
    for (int y = 0; y < icon.height(); ++y) {
      SkColor pixel = bitmap.getColor(x, y);
      if (SkColorGetR(pixel) > kThreshold || SkColorGetB(pixel) > kThreshold ||
          SkColorGetG(pixel) > kThreshold) {
        if (++visible_pixels == minimum_visible_pixels) {
          return true;
        }
      }
    }
  }
  return false;
}

bool RenderIconForVisibilityAnalysis(const SkBitmap& icon,
                                     SkColor background_color,
                                     SkBitmap* rendered_icon) {
  DCHECK(rendered_icon);
  DCHECK(rendered_icon->empty());
  if (icon.width() * icon.height() > kMaxAllowedPixels) {
    return false;
  }
  if (!rendered_icon->tryAllocN32Pixels(icon.width(), icon.height())) {
    LOG(ERROR) << "Unable to allocate pixels for a " << icon.width() << "x"
               << icon.height() << "icon.";
    return false;
  }
  rendered_icon->eraseColor(background_color);
  SkCanvas offscreen(*rendered_icon, SkSurfaceProps{});
  offscreen.drawImage(SkImages::RasterFromBitmap(icon), 0, 0);
  offscreen.drawColor(background_color, SkBlendMode::kDifference);

  return true;
}

bool IsRenderedIconAtPathSufficientlyVisible(const base::FilePath& path,
                                             SkColor background_color) {
  SkBitmap icon;
  if (!LoadPngFromFile(path, &icon)) {
    return false;
  }
  return IsRenderedIconSufficientlyVisible(icon, background_color);
}

bool LoadPngFromFile(const base::FilePath& path, SkBitmap* dst) {
  std::string png_bytes;
  if (!base::ReadFileToString(path, &png_bytes)) {
    return false;
  }
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(png_bytes.data()),
      png_bytes.length(), dst);
}

}  // namespace extensions::image_util
