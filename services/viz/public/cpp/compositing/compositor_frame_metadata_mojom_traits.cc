// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_metadata_mojom_traits.h"

#include <algorithm>

#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/trees_in_viz_timing_mojom_traits.h"
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
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::root_scroll_offset");
    return false;
  }

  out->page_scale_factor = data.page_scale_factor();
  if (!data.ReadScrollableViewportSize(&out->scrollable_viewport_size)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::scrollable_viewport_size");
    return false;
  }
  if (!data.ReadVisibleViewportSize(&out->visible_viewport_size)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::visible_viewport_size");
    return false;
  }

  if (data.frame_token() == viz::kInvalidFrameToken) {
    viz::SetDeserializationCrashKeyString("Invalid frame token");
    return false;
  }
  out->frame_token = data.frame_token();

  if (!data.ReadContentColorUsage(&out->content_color_usage)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::content_color_usage");
    return false;
  }

  if (!data.ReadRootBackgroundColor(&out->root_background_color)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::root_background_color");
    return false;
  }

  out->may_contain_video = data.may_contain_video();
  out->may_throttle_if_undrawn_frames = data.may_throttle_if_undrawn_frames();
  out->has_shared_element_resources = data.has_shared_element_resources();
  out->is_handling_interaction = data.is_handling_interaction();
  out->is_handling_animation = data.is_handling_animation();
  out->send_frame_token_to_embedder = data.send_frame_token_to_embedder();
  out->min_page_scale_factor = data.min_page_scale_factor();
  out->is_mobile_optimized = data.is_mobile_optimized();
  out->is_software = data.is_software();
  out->top_controls_visible_height = data.top_controls_visible_height();

  if (!data.ReadScreenshotDestination(&out->screenshot_destination)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::screenshot_destination");
    return false;
  }

  if (!data.ReadLatencyInfo(&out->latency_info)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::latency_info");
    return false;
  }
  if (!data.ReadReferencedSurfaces(&out->referenced_surfaces)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::referenced_surfaces");
    return false;
  }
  if (!data.ReadDeadline(&out->deadline)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::deadline");
    return false;
  }
  if (!data.ReadActivationDependencies(&out->activation_dependencies)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::activation_dependencies");
    return false;
  }
  if (!data.ReadBeginFrameAck(&out->begin_frame_ack)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::begin_frame_ack");
    return false;
  }
  if (!data.ReadDisplayTransformHint(&out->display_transform_hint)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::display_transform_hint");
    return false;
  }
  if (!data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::delegated_ink_metadata");
    return false;
  }
  if (!data.ReadTransitionDirectives(&out->transition_directives)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::transition_directives");
    return false;
  }
  if (!data.ReadCaptureBounds(&out->capture_bounds)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::capture_bounds");
    return false;
  }
  if (!data.ReadOffsetTagDefinitions(&out->offset_tag_definitions)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::offset_tag_definitions");
    return false;
  }
  if (!data.ReadOffsetTagValues(&out->offset_tag_values)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::offset_tag_values");
    return false;
  }
  if (!data.ReadFrameIntervalInputs(&out->frame_interval_inputs)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::frame_interval_inputs");
    return false;
  }
  if (!data.ReadTreesInVizTiming(&out->trees_in_viz_timing_details)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::trees_in_viz_timing_details");
    return false;
  }
  if (!data.ReadTrackedElementRects(&out->tracked_element_rects)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrameMetadata::tracked_element_rects");
    return false;
  }

  // Verify that OffsetTagDefinition providers are referenced surfaces.
  for (auto& tag_def : out->offset_tag_definitions) {
    if (!std::ranges::contains(out->referenced_surfaces, tag_def.provider)) {
      viz::SetDeserializationCrashKeyString(
          "Offset tag provider not in referenced surfaces");
      return false;
    }
  }

  return true;
}

}  // namespace mojo
