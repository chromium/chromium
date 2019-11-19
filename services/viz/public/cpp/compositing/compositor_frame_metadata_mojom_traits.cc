// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_metadata_mojom_traits.h"

#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::CompositorFrameMetadataDataView,
                  viz::CompositorFrameMetadata>::
    Read(viz::mojom::CompositorFrameMetadataDataView data,
         viz::CompositorFrameMetadata* out) {
  // TODO(samans): Replace CHECKs with early-outs once we find out the cause for
  // deserialization failures. https://crbug.com/979564
  CHECK_GT(data.device_scale_factor(), 0);
  out->device_scale_factor = data.device_scale_factor();
  CHECK(data.ReadRootScrollOffset(&out->root_scroll_offset));

  out->page_scale_factor = data.page_scale_factor();
  CHECK(data.ReadScrollableViewportSize(&out->scrollable_viewport_size));

  CHECK(data.frame_token());
  out->frame_token = data.frame_token();

  out->may_contain_video = data.may_contain_video();
  out->is_resourceless_software_draw_with_scroll_or_animation =
      data.is_resourceless_software_draw_with_scroll_or_animation();
  out->send_frame_token_to_embedder = data.send_frame_token_to_embedder();
  out->root_background_color = data.root_background_color();
  out->min_page_scale_factor = data.min_page_scale_factor();
  out->top_controls_height = data.top_controls_height();
  out->top_controls_shown_ratio = data.top_controls_shown_ratio();
#if defined(OS_ANDROID)
  out->bottom_controls_height = data.bottom_controls_height();
  out->bottom_controls_shown_ratio = data.bottom_controls_shown_ratio();
#endif

  CHECK(data.ReadLatencyInfo(&out->latency_info));
  CHECK(data.ReadReferencedSurfaces(&out->referenced_surfaces));
  CHECK(data.ReadDeadline(&out->deadline));
  CHECK(data.ReadActivationDependencies(&out->activation_dependencies));
  CHECK(data.ReadBeginFrameAck(&out->begin_frame_ack));
  CHECK(data.ReadLocalSurfaceIdAllocationTime(
      &out->local_surface_id_allocation_time));
  CHECK(!out->local_surface_id_allocation_time.is_null());
  CHECK(data.ReadPreferredFrameInterval(&out->preferred_frame_interval));
  return true;
}

}  // namespace mojo
