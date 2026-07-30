// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/hit_test/hit_test_region_list_mojom_traits.h"

#include "services/viz/public/cpp/crash_keys.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::HitTestRegionDataView, viz::HitTestRegion>::Read(
    viz::mojom::HitTestRegionDataView data,
    viz::HitTestRegion* out) {
  if (!data.ReadFrameSinkId(&out->frame_sink_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read HitTestRegion::frame_sink_id");
    return false;
  }
  if (!data.ReadRect(&out->rect)) {
    viz::SetDeserializationCrashKeyString("Failed read HitTestRegion::rect");
    return false;
  }
  if (!data.ReadTransform(&out->transform)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read HitTestRegion::transform");
    return false;
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  return true;
}

// static
bool StructTraits<
    viz::mojom::HitTestRegionListDataView,
    viz::HitTestRegionList>::Read(viz::mojom::HitTestRegionListDataView data,
                                  viz::HitTestRegionList* out) {
  if (!data.ReadRegions(&out->regions))
    return false;
  if (!data.ReadBounds(&out->bounds)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read HitTestRegionList::bounds");
    return false;
  }
  if (!data.ReadTransform(&out->transform)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read HitTestRegionList::transform");
    return false;
  }
  out->flags = data.flags();
  out->async_hit_test_reasons = data.async_hit_test_reasons();
  return true;
}

}  // namespace mojo
