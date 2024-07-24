// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_

#include <memory>
#include <optional>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"
#include "services/viz/public/cpp/compositing/frame_deadline_mojom_traits.h"
#include "services/viz/public/cpp/compositing/frame_interval_inputs_mojom_traits.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"
#include "services/viz/public/cpp/compositing/region_capture_bounds_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_range_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_frame_metadata.mojom-shared.h"
#include "ui/gfx/mojom/delegated_ink_metadata_mojom_traits.h"
#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"
#include "ui/gfx/mojom/overlay_transform_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CompositorFrameMetadataDataView,
                    viz::CompositorFrameMetadata> {
  static float device_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    DCHECK_GT(metadata.device_scale_factor, 0);
    return metadata.device_scale_factor;
  }

  static gfx::PointF root_scroll_offset(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.root_scroll_offset;
  }

  static float page_scale_factor(const viz::CompositorFrameMetadata& metadata) {
    return metadata.page_scale_factor;
  }

  static gfx::SizeF scrollable_viewport_size(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.scrollable_viewport_size;
  }

  static gfx::ContentColorUsage content_color_usage(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.content_color_usage;
  }

  static bool may_contain_video(const viz::CompositorFrameMetadata& metadata) {
    return metadata.may_contain_video;
  }

  static bool may_throttle_if_undrawn_frames(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.may_throttle_if_undrawn_frames;
  }

  static bool has_shared_element_resources(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.has_shared_element_resources;
  }

  static bool is_handling_interaction(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.is_handling_interaction;
  }

  static SkColor4f root_background_color(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.root_background_color;
  }

  static const std::vector<ui::LatencyInfo>& latency_info(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.latency_info;
  }

  static const std::vector<viz::SurfaceRange>& referenced_surfaces(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.referenced_surfaces;
  }

  static const std::vector<viz::SurfaceId>& activation_dependencies(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.activation_dependencies;
  }

  static const viz::FrameDeadline& deadline(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.deadline;
  }

  static const viz::BeginFrameAck& begin_frame_ack(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.begin_frame_ack;
  }

  static uint32_t frame_token(const viz::CompositorFrameMetadata& metadata) {
    DCHECK_GT(metadata.frame_token, 0u);
    return metadata.frame_token;
  }

  static bool send_frame_token_to_embedder(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.send_frame_token_to_embedder;
  }

  static float min_page_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.min_page_scale_factor;
  }

  static bool top_controls_visible_height_set(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_visible_height.has_value();
  }

  static float top_controls_visible_height(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_visible_height.value_or(0.f);
  }

  static gfx::OverlayTransform display_transform_hint(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.display_transform_hint;
  }

  static const std::unique_ptr<gfx::DelegatedInkMetadata>&
  delegated_ink_metadata(const viz::CompositorFrameMetadata& metadata) {
    return metadata.delegated_ink_metadata;
  }

  static const std::vector<viz::CompositorFrameTransitionDirective>&
  transition_directives(const viz::CompositorFrameMetadata& metadata) {
    return metadata.transition_directives;
  }

  static const viz::RegionCaptureBounds& capture_bounds(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.capture_bounds;
  }

  static const std::vector<viz::OffsetTagDefinition>& offset_tag_definitions(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.offset_tag_definitions;
  }

  static const std::vector<viz::OffsetTagValue>& offset_tag_values(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.offset_tag_values;
  }

  static const std::optional<
      blink::SameDocNavigationScreenshotDestinationToken>&
  screenshot_destination(const viz::CompositorFrameMetadata& metadata) {
    return metadata.screenshot_destination;
  }

  static bool is_software(const viz::CompositorFrameMetadata& metadata) {
    return metadata.is_software;
  }

  static const viz::FrameIntervalInputs& frame_interval_inputs(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.frame_interval_inputs;
  }

  static bool Read(viz::mojom::CompositorFrameMetadataDataView data,
                   viz::CompositorFrameMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_
