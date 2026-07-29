// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_mojom_traits.h"

#include "services/viz/public/cpp/crash_keys.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::AggregatedHitTestRegionDataView,
                  viz::AggregatedHitTestRegion>::
    Read(viz::mojom::AggregatedHitTestRegionDataView data,
         viz::AggregatedHitTestRegion* out) {
  if (!data.ReadFrameSinkId(&out->frame_sink_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read AggregatedHitTestRegion::frame_sink_id");
    return false;
  }
  if (!data.ReadRect(&out->rect)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read AggregatedHitTestRegion::rect");
    return false;
  }
  if (!data.ReadTransform(&out->transform)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read AggregatedHitTestRegion::transform");
    return false;
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  out->child_count = data.child_count();
  return true;
}

}  // namespace mojo
