// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/view_transition_state_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::ViewTransitionElementDataView,
                  blink::ViewTransitionElement>::
    Read(blink::mojom::ViewTransitionElementDataView data,
         blink::ViewTransitionElement* out) {
  if (!data.ReadTagName(&out->tag_name) ||
      !data.ReadBorderBoxSizeInCssSpace(&out->border_box_size_in_css_space) ||
      !data.ReadViewportMatrix(&out->viewport_matrix) ||
      !data.ReadOverflowRectInLayoutSpace(
          &out->overflow_rect_in_layout_space) ||
      !data.ReadSnapshotId(&out->snapshot_id) ||
      !data.ReadCapturedRectInLayoutSpace(
          &out->captured_rect_in_layout_space) ||
      !data.ReadCapturedCssProperties(&out->captured_css_properties) ||
      !data.ReadClassList(&out->class_list) ||
      !data.ReadContainingGroupName(&out->containing_group_name)) {
    return false;
  }

  out->paint_order = data.paint_order();
  return true;
}

bool StructTraits<blink::mojom::ViewTransitionStateDataView,
                  blink::ViewTransitionState>::
    Read(blink::mojom::ViewTransitionStateDataView data,
         blink::ViewTransitionState* out) {
  out->device_pixel_ratio = data.device_pixel_ratio();
  out->next_element_resource_id = data.next_element_resource_id();
  return data.ReadElements(&out->elements) &&
         data.ReadTransitionToken(&out->transition_token) &&
         data.ReadSnapshotRootSizeAtCapture(
             &out->snapshot_root_size_at_capture) &&
         data.ReadSubframeSnapshotId(&out->subframe_snapshot_id);
}

}  // namespace mojo
