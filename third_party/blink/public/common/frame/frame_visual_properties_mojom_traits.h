// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/frame_visual_properties.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::FrameVisualPropertiesDataView,
                 blink::FrameVisualProperties> {
  static const blink::ScreenInfo& screen_info(
      const blink::FrameVisualProperties& r) {
    return r.screen_info;
  }

  static bool auto_resize_enabled(const blink::FrameVisualProperties& r) {
    return r.auto_resize_enabled;
  }

  static bool is_pinch_gesture_active(const blink::FrameVisualProperties& r) {
    return r.is_pinch_gesture_active;
  }

  static uint32_t capture_sequence_number(
      const blink::FrameVisualProperties& r) {
    return r.capture_sequence_number;
  }

  static double zoom_level(const blink::FrameVisualProperties& r) {
    return r.zoom_level;
  }

  static double page_scale_factor(const blink::FrameVisualProperties& r) {
    return r.page_scale_factor;
  }

  static const gfx::Size& visible_viewport_size(
      const blink::FrameVisualProperties& r) {
    return r.visible_viewport_size;
  }

  static const gfx::Size& min_size_for_auto_resize(
      const blink::FrameVisualProperties& r) {
    return r.min_size_for_auto_resize;
  }

  static const gfx::Size& max_size_for_auto_resize(
      const blink::FrameVisualProperties& r) {
    return r.max_size_for_auto_resize;
  }

  static const std::vector<gfx::Rect>& root_widget_window_segments(
      const blink::FrameVisualProperties& r) {
    return r.root_widget_window_segments;
  }

  static const gfx::Rect& compositor_viewport(
      const blink::FrameVisualProperties& r) {
    return r.compositor_viewport;
  }

  static const gfx::Rect& screen_space_rect(
      const blink::FrameVisualProperties& r) {
    return r.screen_space_rect;
  }

  static const gfx::Size& local_frame_size(
      const blink::FrameVisualProperties& r) {
    return r.local_frame_size;
  }

  static base::Optional<viz::LocalSurfaceId> local_surface_id(
      const blink::FrameVisualProperties& r) {
    return r.local_surface_id;
  }

  static bool Read(blink::mojom::FrameVisualPropertiesDataView r,
                   blink::FrameVisualProperties* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_MOJOM_TRAITS_H_
