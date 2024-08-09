// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_

#include <optional>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "services/viz/public/mojom/compositing/view_transition_element_resource_id.mojom.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/frame/view_transition_state.h"
#include "third_party/blink/public/mojom/frame/view_transition_state.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ViewTransitionElementDataView,
                 blink::ViewTransitionElement> {
  static const std::string& tag_name(const blink::ViewTransitionElement& r) {
    return r.tag_name;
  }

  static const gfx::SizeF& border_box_size_in_css_space(
      const blink::ViewTransitionElement& r) {
    return r.border_box_size_in_css_space;
  }

  static const gfx::Transform& viewport_matrix(
      const blink::ViewTransitionElement& r) {
    return r.viewport_matrix;
  }

  static const gfx::RectF& overflow_rect_in_layout_space(
      const blink::ViewTransitionElement& r) {
    return r.overflow_rect_in_layout_space;
  }

  static const viz::ViewTransitionElementResourceId& snapshot_id(
      const blink::ViewTransitionElement& r) {
    return r.snapshot_id;
  }

  static int32_t paint_order(const blink::ViewTransitionElement& r) {
    return r.paint_order;
  }

  static const std::optional<gfx::RectF>& captured_rect_in_layout_space(
      const blink::ViewTransitionElement& r) {
    return r.captured_rect_in_layout_space;
  }

  static const base::flat_map<blink::mojom::ViewTransitionPropertyId,
                              std::string>&
  captured_css_properties(const blink::ViewTransitionElement& r) {
    return r.captured_css_properties;
  }

  static const std::vector<std::string>& class_list(
      const blink::ViewTransitionElement& r) {
    return r.class_list;
  }

  static const std::string& containing_group_name(
      const blink::ViewTransitionElement& r) {
    return r.containing_group_name;
  }

  static bool Read(blink::mojom::ViewTransitionElementDataView r,
                   blink::ViewTransitionElement* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ViewTransitionStateDataView,
                 blink::ViewTransitionState> {
  static const std::vector<blink::ViewTransitionElement>& elements(
      const blink::ViewTransitionState& r) {
    return r.elements;
  }

  static const blink::ViewTransitionToken& transition_token(
      const blink::ViewTransitionState& r) {
    return r.transition_token;
  }

  static const gfx::Size& snapshot_root_size_at_capture(
      const blink::ViewTransitionState& r) {
    return r.snapshot_root_size_at_capture;
  }

  static float device_pixel_ratio(const blink::ViewTransitionState& r) {
    return r.device_pixel_ratio;
  }

  static uint32_t next_element_resource_id(
      const blink::ViewTransitionState& r) {
    return r.next_element_resource_id;
  }

  static const viz::ViewTransitionElementResourceId& subframe_snapshot_id(
      const blink::ViewTransitionState& r) {
    return r.subframe_snapshot_id;
  }

  static bool Read(blink::mojom::ViewTransitionStateDataView r,
                   blink::ViewTransitionState* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_
