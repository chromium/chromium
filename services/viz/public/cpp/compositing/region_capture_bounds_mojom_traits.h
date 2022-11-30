// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_REGION_CAPTURE_BOUNDS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_REGION_CAPTURE_BOUNDS_MOJOM_TRAITS_H_

#include <utility>
#include <vector>

#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "services/viz/public/mojom/compositing/region_capture_bounds.mojom-shared.h"

namespace mojo {
// Intermediary struct for mapping RegionCaptureBounds map key value pairs.
struct CropIdBoundsPair {
  base::Token crop_id;
  gfx::Rect bounds;
};
template <>
struct StructTraits<viz::mojom::CropIdBoundsPairDataView, CropIdBoundsPair> {
  static base::Token crop_id(const CropIdBoundsPair& pair) {
    return pair.crop_id;
  }

  static gfx::Rect bounds(const CropIdBoundsPair& pair) { return pair.bounds; }

  static bool Read(viz::mojom::CropIdBoundsPairDataView data,
                   CropIdBoundsPair* out) {
    base::Token crop_id;
    gfx::Rect bounds;
    if (!data.ReadCropId(&crop_id)) {
      return false;
    }
    if (!data.ReadBounds(&bounds)) {
      return false;
    }
    *out = CropIdBoundsPair{std::move(crop_id), std::move(bounds)};
    return true;
  }
};

template <>
struct StructTraits<viz::mojom::RegionCaptureBoundsDataView,
                    viz::RegionCaptureBounds> {
  static std::vector<CropIdBoundsPair> bounds(
      const viz::RegionCaptureBounds& capture_bounds) {
    std::vector<CropIdBoundsPair> out;
    out.reserve(capture_bounds.bounds().size());
    for (const auto& pair : capture_bounds.bounds()) {
      out.push_back(CropIdBoundsPair{pair.first, pair.second});
    }
    return out;
  }

  static bool Read(viz::mojom::RegionCaptureBoundsDataView data,
                   viz::RegionCaptureBounds* out) {
    std::vector<CropIdBoundsPair> token_rects;
    if (!data.ReadBounds(&token_rects)) {
      return false;
    }

    base::flat_map<viz::RegionCaptureCropId, gfx::Rect> bounds;
    for (const auto& pair : token_rects) {
      bounds.emplace(pair.crop_id, pair.bounds);
    }
    *out = viz::RegionCaptureBounds(std::move(bounds));
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_REGION_CAPTURE_BOUNDS_MOJOM_TRAITS_H_
