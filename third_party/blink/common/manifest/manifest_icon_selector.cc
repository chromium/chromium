// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace blink {

namespace {

constexpr const char* kLimitedMimeTypes[] = {"image/png", "image/svg+xml",
                                             "image/webp"};

bool IsIconTypeSupported(const Manifest::ImageResource& icon,
                         bool limited_image_types_for_installable_icon) {
  // The type field is optional. If it isn't present, fall back on checking
  // the src extension.
  std::string mime_type = base::UTF16ToUTF8(icon.type);
  if (mime_type.empty()) {
    net::GetWellKnownMimeTypeFromFile(
        base::FilePath::FromASCII(icon.src.ExtractFileName()), &mime_type);
    if (mime_type.empty()) {
      return false;
    }
  }

  if (limited_image_types_for_installable_icon) {
    for (const char* limited_type : kLimitedMimeTypes) {
      if (mime_type.compare(limited_type) == 0) {
        return true;
      }
    }
    return false;
  }

  return blink::IsSupportedImageMimeType(mime_type) ||
         (mime_type.starts_with("image/") &&
          blink::IsSupportedNonImageMimeType(mime_type));
}

bool IsIconSvg(const Manifest::ImageResource& icon) {
  if (base::EqualsASCII(icon.type, "image/svg+xml")) {
    return true;
  }
  return icon.type.empty() &&
         base::EndsWith(icon.src.ExtractFileName(), ".svg",
                        base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

// static
std::optional<ManifestIconSelectorResult>
ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<blink::Manifest::ImageResource>& icons,
    const ManifestIconSelectorParams& params) {
  DCHECK_GE(params.max_width_to_height_ratio, 1.0f);

  std::optional<ManifestIconSelectorResult> best_raster_icon;
  std::optional<ManifestIconSelectorResult> best_svg_icon;
  std::optional<ManifestIconSelectorResult> fallback_any_icon;
  int best_delta = std::numeric_limits<int>::min();

  for (const auto& icon : icons) {
    if (!std::ranges::contains(icon.purpose, params.purpose) ||
        !IsIconTypeSupported(icon,
                             params.limited_image_types_for_installable_icon)) {
      continue;
    }

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty()) {
        // Handle "any" size icons.
        if (IsIconSvg(icon)) {
          best_svg_icon =
              ManifestIconSelectorResult{icon.src, gfx::Size(), params.purpose};
        } else {
          fallback_any_icon =
              ManifestIconSelectorResult{icon.src, gfx::Size(), params.purpose};
        }
        continue;
      }
      // Skip icons with a single dimension of 0.
      if (size.width() == 0 || size.height() == 0) {
        continue;
      }

      // Check for minimum size.
      if (size.height() < params.minimum_icon_size_in_px) {
        continue;
      }

      if (size.width() > params.maximum_icon_size_in_px ||
          size.height() > params.maximum_icon_size_in_px) {
        continue;
      }

      float width = static_cast<float>(size.width());
      float height = static_cast<float>(size.height());
      DCHECK_GT(height, 0);
      DCHECK_GT(width, 0);
      // Calculate ratio in an orientation-agnostic way.
      float ratio = width > height ? width / height : height / width;
      if (ratio > params.max_width_to_height_ratio) {
        continue;
      }

      int delta = size.height() - params.ideal_icon_size_in_px;
      if (delta < 0 && best_delta > 0) {
        continue;
      }
      if (best_delta == std::numeric_limits<int>::min() ||
          (delta > 0 && best_delta < 0) ||
          std::abs(delta) <= std::abs(best_delta)) {
        best_delta = delta;
        best_raster_icon =
            ManifestIconSelectorResult{icon.src, size, params.purpose};
      }
    }
  }
  // Now, determine which icon to return based on SVG handling policy.
  if (best_raster_icon && best_delta == 0) {
    return best_raster_icon;
  }
  if (params.svg_handling ==
      ManifestIconSelectorParams::AnySizeSvgHandling::kAsSecondPriority) {
    if (best_svg_icon) {
      return best_svg_icon;
    }
    if (fallback_any_icon) {
      return fallback_any_icon;
    }
  }
  if (best_raster_icon) {
    return best_raster_icon;
  }
  if (best_svg_icon) {
    DCHECK_EQ(params.svg_handling,
              ManifestIconSelectorParams::AnySizeSvgHandling::kAsFallback);
    return best_svg_icon;
  }
  if (fallback_any_icon) {
    return fallback_any_icon;
  }

  return std::nullopt;
}

}  // namespace blink
