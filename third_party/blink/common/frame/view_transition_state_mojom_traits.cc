// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/view_transition_state_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "third_party/blink/public/mojom/frame/view_transition_state.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

bool StructTraits<
    blink::mojom::ViewTransitionElementLayeredBoxPropertiesDataView,
    blink::ViewTransitionElement::LayeredBoxProperties>::
    Read(blink::mojom::ViewTransitionElementLayeredBoxPropertiesDataView data,
         blink::ViewTransitionElement::LayeredBoxProperties* out) {
  return data.ReadContentBox(&out->content_box) &&
         data.ReadPaddingBox(&out->padding_box) &&
         data.ReadBoxSizing(&out->box_sizing);
}
bool StructTraits<blink::mojom::ViewTransitionElementDataView,
                  blink::ViewTransitionElement>::
    Read(blink::mojom::ViewTransitionElementDataView data,
         blink::ViewTransitionElement* out) {
  if (!data.ReadTagName(&out->tag_name) ||
      !data.ReadBorderBoxRectInEnclosingLayerCssSpace(
          &out->border_box_rect_in_enclosing_layer_css_space) ||
      !data.ReadViewportMatrix(&out->viewport_matrix) ||
      !data.ReadOverflowRectInLayoutSpace(
          &out->overflow_rect_in_layout_space) ||
      !data.ReadSnapshotId(&out->snapshot_id) ||
      !data.ReadCapturedRectInLayoutSpace(
          &out->captured_rect_in_layout_space) ||
      !data.ReadCapturedCssProperties(&out->captured_css_properties) ||
      !data.ReadGroupChildrenCssProperties(
          &out->group_children_css_properties) ||
      !data.ReadBorderOffset(&out->border_offset) ||
      !data.ReadClassList(&out->class_list) ||
      !data.ReadContainingGroupName(&out->containing_group_name) ||
      !data.ReadLayeredBoxProperties(&out->layered_box_properties)) {
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
  out->delay_layer_tree_view_deletion_ = data.delay_layer_tree_view_deletion();
  return data.ReadElements(&out->elements) &&
         data.ReadTransitionToken(&out->transition_token) &&
         data.ReadSnapshotRootSizeAtCapture(
             &out->snapshot_root_size_at_capture) &&
         data.ReadSubframeSnapshotId(&out->subframe_snapshot_id) &&
         data.ReadIdToAutoNameMap(&out->id_to_auto_name_map);
}

}  // namespace mojo
