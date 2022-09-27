// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"

#include <limits>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace blink {

// static
BLINK_COMMON_EXPORT GURL ManifestIconSelector::FindBestMatchingSquareIcon(
    const std::vector<blink::Manifest::ImageResource>& icons,
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    blink::mojom::ManifestImageResource_Purpose purpose) {
  return FindBestMatchingIcon(icons, ideal_icon_size_in_px,
                              minimum_icon_size_in_px,
                              1 /*max_width_to_height_ratio */, purpose);
}

// static
BLINK_COMMON_EXPORT GURL ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<blink::Manifest::ImageResource>& icons,
    int ideal_icon_height_in_px,
    int minimum_icon_height_in_px,
    float max_width_to_height_ratio,
    blink::mojom::ManifestImageResource_Purpose purpose) {
  DCHECK_LE(minimum_icon_height_in_px, ideal_icon_height_in_px);
  DCHECK_GE(max_width_to_height_ratio, 1.0);

  // Icon with exact matching size has priority over icon with size "any", which
  // has priority over icon with closest matching size.
  int latest_size_any_index = -1;
  int closest_size_match_index = -1;
  int best_delta_in_size = std::numeric_limits<int>::min();

  for (size_t i = 0; i < icons.size(); ++i) {
    const auto& icon = icons[i];

    // Check for supported image MIME types.
    if (!icon.type.empty()) {
      std::string type = base::UTF16ToUTF8(icon.type);
      if (!(blink::IsSupportedImageMimeType(base::UTF16ToUTF8(icon.type)) ||
            // The following condition is intended to support image/svg+xml:
            (base::StartsWith(base::UTF16ToUTF8(icon.type), "image/",
                              base::CompareCase::SENSITIVE) &&
             blink::IsSupportedNonImageMimeType(
                 base::UTF16ToUTF8(icon.type))))) {
        continue;
      }
    }

    // Check for icon purpose.
    if (!base::Contains(icon.purpose, purpose))
      continue;

    // Check for size constraints.
    for (const gfx::Size& size : icon.sizes) {
      // Check for size "any". Return this icon if no better one is found.
      if (size.IsEmpty()) {
        latest_size_any_index = i;
        continue;
      }

      // Check for minimum size.
      if (size.height() < minimum_icon_height_in_px)
        continue;

      // Check for width to height ratio.
      float width = static_cast<float>(size.width());
      float height = static_cast<float>(size.height());
      DCHECK_GT(height, 0);
      float ratio = width / height;
      if (ratio < 1 || ratio > max_width_to_height_ratio)
        continue;

      // According to the spec when there are multiple equally appropriate icons
      // we should choose the last one declared in the list:
      // https://w3c.github.io/manifest/#icons-member
      if (size.height() == ideal_icon_height_in_px) {
        closest_size_match_index = i;
        best_delta_in_size = 0;
        continue;
      }

      // Check for closest match.
      int delta = size.height() - ideal_icon_height_in_px;

      // Smallest icon larger than ideal size has priority over largest icon
      // smaller than ideal size.
      if (best_delta_in_size > 0 && delta < 0)
        continue;

      if ((best_delta_in_size > 0 && delta < best_delta_in_size) ||
          (best_delta_in_size < 0 && delta > best_delta_in_size)) {
        closest_size_match_index = i;
        best_delta_in_size = delta;
      }
    }
  }

  if (best_delta_in_size == 0) {
    DCHECK_NE(closest_size_match_index, -1);
    return icons[closest_size_match_index].src;
  }
  if (latest_size_any_index != -1)
    return icons[latest_size_any_index].src;
  if (closest_size_match_index != -1)
    return icons[closest_size_match_index].src;
  return GURL();
}

}  // namespace blink
