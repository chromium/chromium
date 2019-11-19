// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_HIT_TEST_AGGREGATED_HIT_TEST_REGION_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_HIT_TEST_AGGREGATED_HIT_TEST_REGION_MOJOM_TRAITS_H_

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "services/viz/public/cpp/compositing/frame_sink_id_mojom_traits.h"
#include "services/viz/public/mojom/hit_test/aggregated_hit_test_region.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::AggregatedHitTestRegionDataView,
                    viz::AggregatedHitTestRegion> {
  static const viz::FrameSinkId& frame_sink_id(
      const viz::AggregatedHitTestRegion& region) {
    return region.frame_sink_id;
  }

  static uint32_t flags(const viz::AggregatedHitTestRegion& region) {
    return region.flags;
  }

  static uint32_t async_hit_test_reasons(
      const viz::AggregatedHitTestRegion& region) {
    return region.async_hit_test_reasons;
  }

  static const gfx::Rect& rect(const viz::AggregatedHitTestRegion& region) {
    return region.rect;
  }

  static uint32_t child_count(const viz::AggregatedHitTestRegion& region) {
    return region.child_count;
  }

  static gfx::Transform transform(const viz::AggregatedHitTestRegion& region) {
    return region.transform();
  }

  static bool Read(viz::mojom::AggregatedHitTestRegionDataView data,
                   viz::AggregatedHitTestRegion* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_HIT_TEST_AGGREGATED_HIT_TEST_REGION_MOJOM_TRAITS_H_
