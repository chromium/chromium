// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_mojom_traits.h"

namespace mojo {

// static
base::expected<void, DeserializationError>
StructTraits<viz::mojom::AggregatedHitTestRegionDataView,
             viz::AggregatedHitTestRegion>::
    Read(viz::mojom::AggregatedHitTestRegionDataView data,
         viz::AggregatedHitTestRegion* out) {
  if (!data.ReadFrameSinkId(&out->frame_sink_id)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadRect(&out->rect)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadTransform(&out->transform)) {
    return base::unexpected(DeserializationError());
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  out->child_count = data.child_count();
  return base::ok();
}

}  // namespace mojo
