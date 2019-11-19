// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/hit_test_region_list_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::HitTestRegionDataView, viz::HitTestRegion>::Read(
    viz::mojom::HitTestRegionDataView data,
    viz::HitTestRegion* out) {
  CHECK(data.ReadFrameSinkId(&out->frame_sink_id));
  CHECK(data.ReadRect(&out->rect));
  CHECK(data.ReadTransform(&out->transform));
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  return true;
}

// static
bool StructTraits<
    viz::mojom::HitTestRegionListDataView,
    viz::HitTestRegionList>::Read(viz::mojom::HitTestRegionListDataView data,
                                  viz::HitTestRegionList* out) {
  CHECK(data.ReadRegions(&out->regions));
  CHECK(data.ReadBounds(&out->bounds));
  CHECK(data.ReadTransform(&out->transform));
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  return true;
}

}  // namespace mojo
