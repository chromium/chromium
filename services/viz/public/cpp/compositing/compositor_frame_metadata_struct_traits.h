// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_

#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_struct_traits.h"
#include "services/viz/public/cpp/compositing/frame_deadline_struct_traits.h"
#include "services/viz/public/cpp/compositing/surface_range_struct_traits.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_metadata.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CompositorFrameMetadataDataView,
                    viz::CompositorFrameMetadata> {
  static float device_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.device_scale_factor;
  }

  static gfx::Vector2dF root_scroll_offset(
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

  static bool may_contain_video(const viz::CompositorFrameMetadata& metadata) {
    return metadata.may_contain_video;
  }

  static bool is_resourceless_software_draw_with_scroll_or_animation(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.is_resourceless_software_draw_with_scroll_or_animation;
  }

  static uint32_t root_background_color(
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

  static uint32_t content_source_id(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.content_source_id;
  }

  static const viz::BeginFrameAck& begin_frame_ack(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.begin_frame_ack;
  }

  static uint32_t frame_token(const viz::CompositorFrameMetadata& metadata) {
    return metadata.frame_token;
  }

  static bool send_frame_token_to_embedder(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.send_frame_token_to_embedder;
  }

  static bool request_presentation_feedback(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.request_presentation_feedback;
  }

  static float min_page_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.min_page_scale_factor;
  }

  static float top_controls_height(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_height;
  }

  static float top_controls_shown_ratio(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_shown_ratio;
  }

  static base::TimeTicks local_surface_id_allocation_time(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.local_surface_id_allocation_time;
  }

#if defined(OS_ANDROID)
  static float max_page_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.max_page_scale_factor;
  }

  static const gfx::SizeF& root_layer_size(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.root_layer_size;
  }

  static bool root_overflow_y_hidden(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.root_overflow_y_hidden;
  }

  static float bottom_controls_height(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.bottom_controls_height;
  }

  static float bottom_controls_shown_ratio(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.bottom_controls_shown_ratio;
  }

  static const viz::Selection<gfx::SelectionBound>& selection(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.selection;
  }
#endif  // defined(OS_ANDROID)

  static bool Read(viz::mojom::CompositorFrameMetadataDataView data,
                   viz::CompositorFrameMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_
