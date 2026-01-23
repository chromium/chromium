// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_ICON_SELECTOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_ICON_SELECTOR_H_

#include <limits>
#include <optional>
#include <vector>

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {

// A struct containing the full details of a selected icon.
struct BLINK_COMMON_EXPORT ManifestIconSelectorResult {
  GURL icon_url;
  gfx::Size icon_size;
  mojom::ManifestImageResource_Purpose icon_purpose;
};

// A struct containing the parameters for selecting an icon, allowing callers
// to specify their selection policy.
struct BLINK_COMMON_EXPORT ManifestIconSelectorParams {
  // How to treat SVG icons with `size: "any"`.
  enum class AnySizeSvgHandling {
    // Treat as the second-best match after an exact size match.
    kAsSecondPriority,
    // Only consider SVGs if no suitable raster icon is found.
    kAsFallback,
  };

  // An icon is only eligible if its purpose list includes this `purpose`.
  mojom::ManifestImageResource_Purpose purpose =
      mojom::ManifestImageResource_Purpose::ANY;
  AnySizeSvgHandling svg_handling = AnySizeSvgHandling::kAsSecondPriority;

  // Size constraints and preferences. By default, no min/max, but the icon must
  // be square. If no icon of the ideal size is found, we will prefer the
  // closest larger-than-ideal image within bounds, followed by the closest
  // smaller-than-ideal image within bounds.
  int ideal_icon_size_in_px = 0;
  int minimum_icon_size_in_px = 0;
  int maximum_icon_size_in_px = std::numeric_limits<int>::max();
  float max_width_to_height_ratio = 1.0;

  // If true, only icons that can be used for an installed app are considered.
  // Practically, this means that only png, svg, and webp images are considered.
  bool limited_image_types_for_installable_icon = false;
};

// Selects the landscape or square icon with the supported image MIME types and
// the specified icon purpose that most closely matches the size constraints.
// This follows very basic heuristics -- improvements are welcome.
class BLINK_COMMON_EXPORT ManifestIconSelector {
 public:
  ManifestIconSelector() = delete;
  ManifestIconSelector(const ManifestIconSelector&) = delete;
  ManifestIconSelector& operator=(const ManifestIconSelector&) = delete;

  // Selects the best matching icon from the manifest based on a flexible set
  // of parameters.
  static std::optional<ManifestIconSelectorResult> FindBestMatchingIcon(
      const std::vector<blink::Manifest::ImageResource>& icons,
      const ManifestIconSelectorParams& params);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_ICON_SELECTOR_H_
