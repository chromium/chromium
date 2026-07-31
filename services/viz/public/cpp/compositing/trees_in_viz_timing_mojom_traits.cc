// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/trees_in_viz_timing_mojom_traits.h"

#include "services/viz/public/cpp/crash_keys.h"
#include "services/viz/public/mojom/compositing/trees_in_viz_timing.mojom.h"

namespace mojo {

using Traits =
    StructTraits<viz::mojom::TreesInVizTimingDataView, viz::TreesInVizTiming>;
// static
bool Traits::Read(viz::mojom::TreesInVizTimingDataView data,
                  viz::TreesInVizTiming* out) {
  if (!data.ReadStartUpdateDisplayTree(&out->start_update_display_tree)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TreesInVizTiming::start_update_display_tree");
    return false;
  }
  if (!data.ReadStartPrepareToDraw(&out->start_prepare_to_draw)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TreesInVizTiming::start_prepare_to_draw");
    return false;
  }
  if (!data.ReadStartDrawLayers(&out->start_draw_layers)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TreesInVizTiming::start_draw_layers");
    return false;
  }
  if (!data.ReadSubmitCompositorFrame(&out->submit_compositor_frame)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TreesInVizTiming::submit_compositor_frame");
    return false;
  }
  if (!(out->start_update_display_tree <= out->start_prepare_to_draw &&
        out->start_prepare_to_draw <= out->start_draw_layers &&
        out->start_draw_layers <= out->submit_compositor_frame)) {
    viz::SetDeserializationCrashKeyString(
        "Invalid timestamps in TreesInVizTiming");
    return false;
  }
  return true;
}

}  // namespace mojo
