// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_
#define CONTENT_COMMON_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/common/render_frame_metadata.mojom-shared.h"
#include "services/viz/public/cpp/compositing/local_surface_id_allocation_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::RenderFrameMetadataDataView,
                    cc::RenderFrameMetadata> {
  static SkColor root_background_color(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_background_color;
  }

  static base::Optional<gfx::Vector2dF> root_scroll_offset(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_scroll_offset;
  }

  static bool is_scroll_offset_at_top(const cc::RenderFrameMetadata& metadata) {
    return metadata.is_scroll_offset_at_top;
  }

  static const viz::Selection<gfx::SelectionBound>& selection(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.selection;
  }

  static bool is_mobile_optimized(const cc::RenderFrameMetadata& metadata) {
    return metadata.is_mobile_optimized;
  }

  static float device_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.device_scale_factor;
  }

  static const gfx::Size& viewport_size_in_pixels(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.viewport_size_in_pixels;
  }

  static const base::Optional<viz::LocalSurfaceIdAllocation>&
  local_surface_id_allocation(const cc::RenderFrameMetadata& metadata) {
    return metadata.local_surface_id_allocation;
  }

  static float page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.page_scale_factor;
  }

  static float external_page_scale_factor(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.external_page_scale_factor;
  }

  static float top_controls_height(const cc::RenderFrameMetadata& metadata) {
    return metadata.top_controls_height;
  }

  static float top_controls_shown_ratio(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.top_controls_shown_ratio;
  }

  static viz::VerticalScrollDirection new_vertical_scroll_direction(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.new_vertical_scroll_direction;
  }

#if defined(OS_ANDROID)
  static float bottom_controls_height(const cc::RenderFrameMetadata& metadata) {
    return metadata.bottom_controls_height;
  }

  static float bottom_controls_shown_ratio(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.bottom_controls_shown_ratio;
  }

  static float min_page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.min_page_scale_factor;
  }

  static float max_page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.max_page_scale_factor;
  }

  static bool root_overflow_y_hidden(const cc::RenderFrameMetadata& metadata) {
    return metadata.root_overflow_y_hidden;
  }

  static const gfx::SizeF& scrollable_viewport_size(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.scrollable_viewport_size;
  }

  static const gfx::SizeF& root_layer_size(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_layer_size;
  }

  static bool has_transparent_background(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.has_transparent_background;
  }
#endif

  static bool Read(content::mojom::RenderFrameMetadataDataView data,
                   cc::RenderFrameMetadata* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_
