// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/hit_test_region_list_mojom_traits.h"

namespace mojo {

// static
base::expected<void, DeserializationError>
StructTraits<viz::mojom::HitTestRegionDataView, viz::HitTestRegion>::Read(
    viz::mojom::HitTestRegionDataView data,
    viz::HitTestRegion* out) {
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
  return base::ok();
}

// static
base::expected<void, DeserializationError> StructTraits<
    viz::mojom::HitTestRegionListDataView,
    viz::HitTestRegionList>::Read(viz::mojom::HitTestRegionListDataView data,
                                  viz::HitTestRegionList* out) {
  if (!data.ReadRegions(&out->regions)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadBounds(&out->bounds)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadTransform(&out->transform)) {
    return base::unexpected(DeserializationError());
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  return base::ok();
}

}  // namespace mojo
