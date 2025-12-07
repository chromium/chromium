// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TREES_IN_VIZ_TIMING_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TREES_IN_VIZ_TIMING_MOJOM_TRAITS_H_

#include "components/viz/common/quads/trees_in_viz_timing.h"
#include "services/viz/public/mojom/compositing/trees_in_viz_timing.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::TreesInVizTimingDataView,
                    viz::TreesInVizTiming> {
  static base::TimeTicks start_update_display_tree(
      const viz::TreesInVizTiming& frame_timing_details) {
    return frame_timing_details.start_update_display_tree;
  }

  static base::TimeTicks start_prepare_to_draw(
      const viz::TreesInVizTiming& frame_timing_details) {
    return frame_timing_details.start_prepare_to_draw;
  }

  static base::TimeTicks start_draw_layers(
      const viz::TreesInVizTiming& frame_timing_details) {
    return frame_timing_details.start_draw_layers;
  }

  static base::TimeTicks submit_compositor_frame(
      const viz::TreesInVizTiming& frame_timing_details) {
    return frame_timing_details.submit_compositor_frame;
  }

  static bool Read(viz::mojom::TreesInVizTimingDataView data,
                   viz::TreesInVizTiming* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TREES_IN_VIZ_TIMING_MOJOM_TRAITS_H_
