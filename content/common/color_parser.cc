// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/color_parser.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/color_utils.h"

namespace content {
namespace {

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
  hsl.s = std::clamp(saturation, 0.0, 100.0) / 100.0;
  hsl.l = std::clamp(lightness, 0.0, 100.0) / 100.0;

  SkAlpha sk_alpha = std::clamp(alpha, 0.0, 1.0) * 255;

  *result = color_utils::HSLToSkColor(hsl, sk_alpha);
  return true;
}

}  // anonymous namespace

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
  if (base::Contains(color_string, '%')) {
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

}  // namespace content
