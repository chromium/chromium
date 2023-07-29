// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_H_

#include "base/unguessable_token.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/public/common/common_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

struct BLINK_COMMON_EXPORT ViewTransitionElement {
  std::string tag_name;
  gfx::SizeF border_box_size_in_css_space;
  gfx::Transform viewport_matrix;
  gfx::RectF overflow_rect_in_layout_space;
  viz::ViewTransitionElementResourceId snapshot_id;
  int32_t paint_order = 0;
  absl::optional<gfx::RectF> captured_rect_in_layout_space;
  uint8_t container_writing_mode = 0;
  uint8_t mix_blend_mode = 0;
  uint8_t text_orientation = 0;
};

struct BLINK_COMMON_EXPORT ViewTransitionState {
  std::vector<ViewTransitionElement> elements;
  base::UnguessableToken navigation_id;
  gfx::Size snapshot_root_size_at_capture;
  float device_pixel_ratio = 1.f;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_H_
