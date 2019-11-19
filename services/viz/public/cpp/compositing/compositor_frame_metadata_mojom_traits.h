// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_

#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/frame_deadline_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_range_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_frame_metadata.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CompositorFrameMetadataDataView,
                    viz::CompositorFrameMetadata> {
  static float device_scale_factor(
      const viz::CompositorFrameMetadata& metadata) {
    DCHECK_GT(metadata.device_scale_factor, 0);
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

  static float top_controls_height(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_height;
  }

  static float top_controls_shown_ratio(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.top_controls_shown_ratio;
  }

#if defined(OS_ANDROID)
  static float bottom_controls_height(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.bottom_controls_height;
  }

  static float bottom_controls_shown_ratio(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.bottom_controls_shown_ratio;
  }
#endif  // defined(OS_ANDROID)

  static base::TimeTicks local_surface_id_allocation_time(
      const viz::CompositorFrameMetadata& metadata) {
    DCHECK(!metadata.local_surface_id_allocation_time.is_null());
    return metadata.local_surface_id_allocation_time;
  }

  static base::Optional<base::TimeDelta> preferred_frame_interval(
      const viz::CompositorFrameMetadata& metadata) {
    return metadata.preferred_frame_interval;
  }

  static bool Read(viz::mojom::CompositorFrameMetadataDataView data,
                   viz::CompositorFrameMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_METADATA_MOJOM_TRAITS_H_
