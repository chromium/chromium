// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_metadata_mojom_traits.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"
#include "skia/public/mojom/skcolor4f_mojom_traits.h"
#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
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

  if (!data.ReadRootBackgroundColor(&out->root_background_color))
    return false;

  out->may_contain_video = data.may_contain_video();
  out->may_throttle_if_undrawn_frames = data.may_throttle_if_undrawn_frames();
  out->has_shared_element_resources = data.has_shared_element_resources();
  out->is_handling_interaction = data.is_handling_interaction();
  out->send_frame_token_to_embedder = data.send_frame_token_to_embedder();
  out->min_page_scale_factor = data.min_page_scale_factor();
  out->is_software = data.is_software();
  if (data.top_controls_visible_height_set()) {
    out->top_controls_visible_height.emplace(
        data.top_controls_visible_height());
  }

  if (!data.ReadScreenshotDestination(&out->screenshot_destination)) {
    return false;
  }

  if (!(data.ReadLatencyInfo(&out->latency_info) &&
        data.ReadReferencedSurfaces(&out->referenced_surfaces) &&
        data.ReadDeadline(&out->deadline) &&
        data.ReadActivationDependencies(&out->activation_dependencies) &&
        data.ReadBeginFrameAck(&out->begin_frame_ack) &&
        data.ReadDisplayTransformHint(&out->display_transform_hint) &&
        data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata) &&
        data.ReadTransitionDirectives(&out->transition_directives) &&
        data.ReadCaptureBounds(&out->capture_bounds) &&
        data.ReadOffsetTagDefinitions(&out->offset_tag_definitions) &&
        data.ReadOffsetTagValues(&out->offset_tag_values) &&
        data.ReadFrameIntervalInputs(&out->frame_interval_inputs))) {
    return false;
  }

  // Verify that OffsetTagDefinition providers are referenced surfaces.
  for (auto& tag_def : out->offset_tag_definitions) {
    if (!base::Contains(out->referenced_surfaces, tag_def.provider)) {
      return false;
    }
  }

  return true;
}

}  // namespace mojo
