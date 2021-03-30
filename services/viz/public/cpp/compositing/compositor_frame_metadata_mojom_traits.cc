// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_metadata_mojom_traits.h"

#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"
#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::CompositorFrameMetadataDataView,
                  viz::CompositorFrameMetadata>::
    Read(viz::mojom::CompositorFrameMetadataDataView data,
         viz::CompositorFrameMetadata* out) {
  if (data.device_scale_factor() <= 0) {
    viz::SetDeserializationCrashKeyString("Invalid device scale factor");
    return false;
  }
  out->device_scale_factor = data.device_scale_factor();
  if (!data.ReadRootScrollOffset(&out->root_scroll_offset)) {
    return false;
  }

  out->page_scale_factor = data.page_scale_factor();
  if (!data.ReadScrollableViewportSize(&out->scrollable_viewport_size))
    return false;

  if (data.frame_token() == 0u)
    return false;
  out->frame_token = data.frame_token();

  if (!data.ReadContentColorUsage(&out->content_color_usage))
    return false;
  out->may_contain_video = data.may_contain_video();
  out->is_resourceless_software_draw_with_scroll_or_animation =
      data.is_resourceless_software_draw_with_scroll_or_animation();
  out->send_frame_token_to_embedder = data.send_frame_token_to_embedder();
  out->root_background_color = data.root_background_color();
  out->min_page_scale_factor = data.min_page_scale_factor();
  if (data.top_controls_visible_height_set()) {
    out->top_controls_visible_height.emplace(
        data.top_controls_visible_height());
  }

  return data.ReadLatencyInfo(&out->latency_info) &&
         data.ReadReferencedSurfaces(&out->referenced_surfaces) &&
         data.ReadDeadline(&out->deadline) &&
         data.ReadActivationDependencies(&out->activation_dependencies) &&
         data.ReadBeginFrameAck(&out->begin_frame_ack) &&
         data.ReadPreferredFrameInterval(&out->preferred_frame_interval) &&
         data.ReadDisplayTransformHint(&out->display_transform_hint) &&
         data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata) &&
         data.ReadTransitionDirectives(&out->transition_directives);
}

}  // namespace mojo
