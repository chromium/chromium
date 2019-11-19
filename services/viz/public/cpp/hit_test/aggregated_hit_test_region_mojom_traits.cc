// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::AggregatedHitTestRegionDataView,
                  viz::AggregatedHitTestRegion>::
    Read(viz::mojom::AggregatedHitTestRegionDataView data,
         viz::AggregatedHitTestRegion* out) {
  if (!data.ReadFrameSinkId(&out->frame_sink_id) ||
      !data.ReadRect(&out->rect) || !data.ReadTransform(&out->transform_)) {
    return false;
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  out->child_count = data.child_count();
  return true;
}

}  // namespace mojo
