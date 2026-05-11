// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_

#include <map>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "mojo/public/cpp/bindings/map_traits_absl.h"
#include "services/viz/public/mojom/compositing/tracked_element_rects.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::TrackedElementRectDataView,
                    viz::TrackedElementRect> {
  static base::Token tracked_element_id(const viz::TrackedElementRect& data) {
    return data.id;
  }

  static const gfx::Rect& visible_bounds(const viz::TrackedElementRect& data) {
    return data.visible_bounds;
  }

  static bool should_add_to_compositor_frame_metadata(
      const viz::TrackedElementRect& data) {
    return data.should_add_to_compositor_frame_metadata;
  }

  static std::optional<blink::FrameToken> frame_token(
      const viz::TrackedElementRect& data) {
    return data.frame_token;
  }

  static std::optional<blink::LocalFrameToken> parent_frame_token(
      const viz::TrackedElementRect& data) {
    return data.parent_frame_token;
  }

  static bool Read(viz::mojom::TrackedElementRectDataView data,
                   viz::TrackedElementRect* out) {
    out->should_add_to_compositor_frame_metadata =
        data.should_add_to_compositor_frame_metadata();
    if (!data.ReadTrackedElementId(&out->id) ||
        !data.ReadVisibleBounds(&out->visible_bounds) ||
        !data.ReadFrameToken(&out->frame_token) ||
        !data.ReadParentFrameToken(&out->parent_frame_token)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<viz::mojom::TrackedElementRectsDataView,
                    viz::TrackedElementRects> {
  // TODO: 491757139 - Simplify the serialization/deserialization to avoid
  // making a copy of the map. Also see: crbug.com/40752610.
  static absl::flat_hash_map<int32_t, std::vector<viz::TrackedElementRect>>
  element_data(const viz::TrackedElementRects& data) {
    absl::flat_hash_map<int32_t, std::vector<viz::TrackedElementRect>> map;
    for (const auto& [feature, rects] : data) {
      map.emplace(static_cast<int32_t>(feature), rects);
    }
    return map;
  }

  static bool Read(viz::mojom::TrackedElementRectsDataView data,
                   viz::TrackedElementRects* out) {
    absl::flat_hash_map<int32_t, std::vector<viz::TrackedElementRect>> int_map;
    if (!data.ReadElementData(&int_map)) {
      return false;
    }
    out->reserve(int_map.size());
    for (const auto& [int_val, rects] : int_map) {
      out->emplace(static_cast<viz::TrackedElementFeature>(int_val),
                   std::move(rects));
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_
