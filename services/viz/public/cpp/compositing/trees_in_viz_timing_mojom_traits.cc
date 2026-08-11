// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/trees_in_viz_timing_mojom_traits.h"

#include "services/viz/public/mojom/compositing/trees_in_viz_timing.mojom.h"

namespace mojo {

using Traits =
    StructTraits<viz::mojom::TreesInVizTimingDataView, viz::TreesInVizTiming>;
// static
base::expected<void, DeserializationError> Traits::Read(
    viz::mojom::TreesInVizTimingDataView data,
    viz::TreesInVizTiming* out) {
  if (!data.ReadStartUpdateDisplayTree(&out->start_update_display_tree)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadStartPrepareToDraw(&out->start_prepare_to_draw)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadStartDrawLayers(&out->start_draw_layers)) {
    return base::unexpected(DeserializationError());
  }
  if (!data.ReadSubmitCompositorFrame(&out->submit_compositor_frame)) {
    return base::unexpected(DeserializationError());
  }
  if (!(out->start_update_display_tree <= out->start_prepare_to_draw &&
        out->start_prepare_to_draw <= out->start_draw_layers &&
        out->start_draw_layers <= out->submit_compositor_frame)) {
    return base::unexpected(DeserializationError());
  }
  return base::ok();
}

}  // namespace mojo
