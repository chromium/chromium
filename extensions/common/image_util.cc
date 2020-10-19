// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/image_util.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"

namespace extensions {
namespace image_util {

bool ParseCssColorString(const std::string& color_string, SkColor* result) {
  if (color_string.empty())
    return false;
  if (color_string[0] == '#')
    return ParseHexColorString(color_string, result);
  if (base::StartsWith(color_string, "hsl", base::CompareCase::SENSITIVE))
    return ParseHslColorString(color_string, result);
  if (base::StartsWith(color_string, "rgb", base::CompareCase::SENSITIVE)) {
    return ParseRgbColorString(color_string, result);
  }
  if (SkParse::FindNamedColor(color_string.c_str(), color_string.size(),
                              result) != nullptr) {
    return true;
  }

  return false;
}

bool ParseHexColorString(const std::string& color_string, SkColor* result) {
  std::string formatted_color;
  // Save a memory allocation -- we never need more than 8 chars.
  formatted_color.reserve(8);

  // Check the string for incorrect formatting.
  if (color_string.empty() || color_string[0] != '#')
    return false;

  // Convert the string from #FFF format to #FFFFFF format.
  if (color_string.length() == 4 || color_string.length() == 5) {
    for (size_t i = 1; i < color_string.length(); ++i) {
      formatted_color += color_string[i];
      formatted_color += color_string[i];
    }
  } else if (color_string.length() == 7) {
    formatted_color.assign(color_string, 1, 6);
  } else if (color_string.length() == 9) {
    formatted_color.assign(color_string, 1, 8);
  } else {
    return false;
  }

  // Add an alpha if one was not set.
  if (formatted_color.length() == 6) {
    formatted_color += "FF";
  }

  // Convert the hex string to an integer.
  std::array<uint8_t, 4> color_bytes;
  if (!base::HexStringToSpan(formatted_color, color_bytes))
    return false;

  *result = SkColorSetARGB(color_bytes[3], color_bytes[0], color_bytes[1],
                           color_bytes[2]);
  return true;
}

std::string GenerateHexColorString(SkColor color) {
  return base::StringPrintf("#%02X%02X%02X", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}

bool ParseRgbColorString(const std::string& color_string, SkColor* result) {
  // https://www.w3.org/wiki/CSS/Properties/color/RGB#The_format_of_the_RGB_Value
  // The CSS3 specification defines the format of a RGB color as
  // rgb(<number>, <number>, <number>) or
  // rgb(<percent>, <percent>, <percent>) or
  // and with alpha, the format is
  // rgb(<number>, <number>, <number>, <alphavalue>) or
  // rgba(<percent>, <percent>, <percent>, <alphavalue>)
  // Whitespace is arbitrary.
  // e.g.: rgb(120, 100, 50), rgba(120, 100, 50, 0.5);
  int r = 0;
  int g = 0;
  int b = 0;
  // 'rgb()' has '1' alpha value implicitly.
  double alpha = 1.0;

  // Percentage rgb values are not supported.
  if (color_string.find('%') != std::string::npos) {
    NOTIMPLEMENTED();
    return false;
  }

  if (!re2::RE2::FullMatch(color_string,
                           "rgb\\(([\\d]+),\\s*([\\d]+),\\s*([\\d]+)\\)", &r,
                           &g, &b) &&
      !re2::RE2::FullMatch(
          color_string,
          "rgba\\(([\\d]+),\\s*([\\d]+),\\s*([\\d]+),\\s*([\\d.]+)\\)", &r, &g,
          &b, &alpha)) {
    return false;
  }

  if (alpha < 0 || alpha > 1.0 || r < 0 || r > 255 || g < 0 || g > 255 ||
      b < 0 || b > 255) {
    return false;
  }

  SkAlpha sk_alpha = alpha * 255;
  *result = SkColorSetARGB(sk_alpha, r, g, b);

  return true;
}

bool ParseHslColorString(const std::string& color_string, SkColor* result) {
  // http://www.w3.org/wiki/CSS/Properties/color/HSL#The_format_of_the_HSL_Value
  // The CSS3 specification defines the format of a HSL color as
  // hsl(<number>, <percent>, <percent>)
  // and with alpha, the format is
  // hsla(<number>, <percent>, <percent>, <number>)
  // e.g.: hsl(120, 100%, 50%), hsla(120, 100%, 50%, 0.5);
  double hue = 0.0;
  double saturation = 0.0;
  double lightness = 0.0;
  // 'hsl()' has '1' alpha value implicitly.
  double alpha = 1.0;
  if (!re2::RE2::FullMatch(color_string,
                           "hsl\\((-?[\\d.]+),\\s*([\\d.]+)%,\\s*([\\d.]+)%\\)",
                           &hue, &saturation, &lightness) &&
      !re2::RE2::FullMatch(
          color_string,
          "hsla\\((-?[\\d.]+),\\s*([\\d.]+)%,\\s*([\\d.]+)%,\\s*([\\d.]+)\\)",
          &hue, &saturation, &lightness, &alpha)) {
    return false;
  }

  color_utils::HSL hsl;
  // Normalize the value between 0.0 and 1.0.
  hsl.h = (((static_cast<int>(hue) % 360) + 360) % 360) / 360.0;
  hsl.s = base::ClampToRange(saturation, 0.0, 100.0) / 100.0;
  hsl.l = base::ClampToRange(lightness, 0.0, 100.0) / 100.0;

  SkAlpha sk_alpha = base::ClampToRange(alpha, 0.0, 1.0) * 255;

  *result = color_utils::HSLToSkColor(hsl, sk_alpha);
  return true;
}

bool IsIconSufficientlyVisible(const SkBitmap& bitmap) {
  // TODO(crbug.com/805600): Currently, we only consider if there are enough
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
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(5),
        50);
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
  // threshold is considered visible.
  SkBitmap bitmap;
  RenderIconForVisibilityAnalysis(icon, background_color, &bitmap);

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

void RenderIconForVisibilityAnalysis(const SkBitmap& icon,
                                     SkColor background_color,
                                     SkBitmap* rendered_icon) {
  DCHECK(rendered_icon);
  DCHECK(rendered_icon->empty());
  rendered_icon->allocN32Pixels(icon.width(), icon.height());
  rendered_icon->eraseColor(background_color);
  SkCanvas offscreen(*rendered_icon);
  offscreen.drawImage(SkImage::MakeFromBitmap(icon), 0, 0);
  offscreen.drawColor(background_color, SkBlendMode::kDifference);
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

}  // namespace image_util
}  // namespace extensions
