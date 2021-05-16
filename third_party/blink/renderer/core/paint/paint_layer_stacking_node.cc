/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

// FIXME: This should not require PaintLayer. There is currently a cycle where
// in order to determine if we isStacked() we have to ask the paint
// layer about some of its state.
PaintLayerStackingNode::PaintLayerStackingNode(PaintLayer& layer)
    : layer_(layer) {
  DCHECK(layer.GetLayoutObject().IsStackingContext());
}

PaintLayerStackingNode::~PaintLayerStackingNode() {
#if DCHECK_IS_ON()
  if (!layer_.GetLayoutObject().DocumentBeingDestroyed())
    UpdateStackingParentForZOrderLists(nullptr);
#endif
}

PaintLayerCompositor* PaintLayerStackingNode::Compositor() const {
  DCHECK(layer_.GetLayoutObject().View());
  if (!layer_.GetLayoutObject().View())
    return nullptr;
  return layer_.GetLayoutObject().View()->Compositor();
}

void PaintLayerStackingNode::DirtyZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_.LayerListMutationAllowed());
  UpdateStackingParentForZOrderLists(nullptr);
#endif

  pos_z_order_list_.clear();
  neg_z_order_list_.clear();

  for (auto& entry :
       layer_to_overlay_overflow_controls_painting_after_.Values()) {
    for (PaintLayer* layer : entry)
      layer->SetNeedsReorderOverlayOverflowControls(false);
  }
  layer_to_overlay_overflow_controls_painting_after_.clear();
  overlay_overflow_controls_reordered_list_.clear();

  z_order_lists_dirty_ = true;

  if (!layer_.GetLayoutObject().DocumentBeingDestroyed() && Compositor())
    Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
}

static bool ZIndexLessThan(const PaintLayer* first, const PaintLayer* second) {
  DCHECK(first->GetLayoutObject().IsStacked());
  DCHECK(second->GetLayoutObject().IsStacked());
  return first->GetLayoutObject().StyleRef().EffectiveZIndex() <
         second->GetLayoutObject().StyleRef().EffectiveZIndex();
}

static bool SetIfHigher(const PaintLayer*& first, const PaintLayer* second) {
  if (!second)
    return false;
  DCHECK_GE(second->GetLayoutObject().StyleRef().EffectiveZIndex(), 0);
  // |second| appears later in the tree, so it's higher than |first| if its
  // z-index >= |first|'s z-index.
  if (!first || !ZIndexLessThan(second, first)) {
    first = second;
    return true;
  }
  return false;
}

// For finding the proper z-order of reparented overlay overflow controls.
struct PaintLayerStackingNode::HighestLayers {
  enum LayerType {
    kAbsolutePosition,
    kFixedPosition,
    kInFlowStacked,
    kLayerTypeCount
  };
  std::array<const PaintLayer*, kLayerTypeCount> highest_layers = {
      nullptr, nullptr, nullptr};
  Vector<LayerType, kLayerTypeCount> highest_layers_order;

  void UpdateOrderForSubtreeHighestLayers(LayerType type,
                                          const PaintLayer* layer) {
    if (SetIfHigher(highest_layers[type], layer)) {
      auto* new_end = std::remove(highest_layers_order.begin(),
                                  highest_layers_order.end(), type);
      if (new_end != highest_layers_order.end()) {
        // |highest_layers_order| doesn't have duplicate elements, std::remove
        // will find at most one element at a time. So we don't shrink it and
        // just update the value of the |new_end|.
        DCHECK(new_end + 1 == highest_layers_order.end());
        *new_end = type;
      } else {
        highest_layers_order.push_back(type);
      }
    }
  }

  void Update(const PaintLayer& layer) {
    const auto& style = layer.GetLayoutObject().StyleRef();
    // We only need to consider zero or positive z-index stacked child for
    // candidates of causing reparent of overlay scrollbars of ancestors.
    // A negative z-index child will not cause reparent of overlay scrollbars
    // because the ancestor scroller either has auto z-index which is above
    // the child or has negative z-index which is a stacking context.
    if (!layer.GetLayoutObject().IsStacked() || style.EffectiveZIndex() < 0)
      return;

    if (style.GetPosition() == EPosition::kAbsolute)
      UpdateOrderForSubtreeHighestLayers(kAbsolutePosition, &layer);
    else if (style.GetPosition() == EPosition::kFixed)
      UpdateOrderForSubtreeHighestLayers(kFixedPosition, &layer);
    else
      UpdateOrderForSubtreeHighestLayers(kInFlowStacked, &layer);
  }

  void Merge(HighestLayers& child) {
    for (auto layer_type : child.highest_layers_order) {
      UpdateOrderForSubtreeHighestLayers(layer_type,
                                         child.highest_layers[layer_type]);
    }
  }
};

void PaintLayerStackingNode::RebuildZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_.LayerListMutationAllowed());
#endif
  DCHECK(z_order_lists_dirty_);

  layer_.SetNeedsReorderOverlayOverflowControls(false);
  for (PaintLayer* child = layer_.FirstChild(); child;
       child = child->NextSibling())
    CollectLayers(*child, nullptr);

  // Sort the two lists.
  std::stable_sort(pos_z_order_list_.begin(), pos_z_order_list_.end(),
                   ZIndexLessThan);
  std::stable_sort(neg_z_order_list_.begin(), neg_z_order_list_.end(),
                   ZIndexLessThan);

  // Append layers for top layer elements after normal layer collection, to
  // ensure they are on top regardless of z-indexes.  The layoutObjects of top
  // layer elements are children of the view, sorted in top layer stacking
  // order.
  if (layer_.IsRootLayer()) {
    LayoutBlockFlow* root_block = layer_.GetLayoutObject().View();
    // If the viewport is paginated, everything (including "top-layer" elements)
    // gets redirected to the flow thread. So that's where we have to look, in
    // that case.
    if (LayoutBlockFlow* multi_column_flow_thread =
            root_block->MultiColumnFlowThread())
      root_block = multi_column_flow_thread;
    for (LayoutObject* child = root_block->FirstChild(); child;
         child = child->NextSibling()) {
      auto* child_element = DynamicTo<Element>(child->GetNode());
      if (child_element && child_element->IsInTopLayer() &&
          child->IsStacked()) {
        pos_z_order_list_.push_back(To<LayoutBoxModelObject>(child)->Layer());
      }
    }
  }

#if DCHECK_IS_ON()
  UpdateStackingParentForZOrderLists(this);
#endif

  z_order_lists_dirty_ = false;
}

void PaintLayerStackingNode::CollectLayers(PaintLayer& paint_layer,
                                           HighestLayers* highest_layers) {
  paint_layer.SetNeedsReorderOverlayOverflowControls(false);

  if (paint_layer.IsInTopLayer())
    return;

  if (highest_layers)
    highest_layers->Update(paint_layer);

  const auto& object = paint_layer.GetLayoutObject();
  const auto& style = object.StyleRef();

  if (object.IsStacked()) {
    auto& list =
        style.EffectiveZIndex() >= 0 ? pos_z_order_list_ : neg_z_order_list_;
    list.push_back(&paint_layer);
  }

  if (object.IsStackingContext())
    return;

  absl::optional<HighestLayers> subtree_highest_layers;
  bool has_overlay_overflow_controls =
      paint_layer.GetScrollableArea() &&
      paint_layer.GetScrollableArea()->HasOverlayOverflowControls();
  if (has_overlay_overflow_controls)
    subtree_highest_layers.emplace();

  for (PaintLayer* child = paint_layer.FirstChild(); child;
       child = child->NextSibling()) {
    CollectLayers(*child, subtree_highest_layers ? &*subtree_highest_layers
                                                 : highest_layers);
  }

  if (has_overlay_overflow_controls) {
    const PaintLayer* layer_to_paint_overlay_overflow_controls_after = nullptr;
    for (auto layer_type : subtree_highest_layers->highest_layers_order) {
      if (layer_type == HighestLayers::kFixedPosition &&
          !object.CanContainFixedPositionObjects())
        continue;
      if (layer_type == HighestLayers::kAbsolutePosition &&
          !object.CanContainAbsolutePositionObjects())
        continue;
      SetIfHigher(layer_to_paint_overlay_overflow_controls_after,
                  subtree_highest_layers->highest_layers[layer_type]);
    }

    if (layer_to_paint_overlay_overflow_controls_after) {
      layer_to_overlay_overflow_controls_painting_after_
          .insert(layer_to_paint_overlay_overflow_controls_after, PaintLayers())
          .stored_value->value.push_back(&paint_layer);
      overlay_overflow_controls_reordered_list_.push_back(&paint_layer);
    }
    paint_layer.SetNeedsReorderOverlayOverflowControls(
        !!layer_to_paint_overlay_overflow_controls_after);

    if (highest_layers)
      highest_layers->Merge(*subtree_highest_layers);
  }
}

#if DCHECK_IS_ON()
void PaintLayerStackingNode::UpdateStackingParentForZOrderLists(
    PaintLayerStackingNode* stacking_parent) {
  for (auto* layer : pos_z_order_list_)
    layer->SetStackingParent(stacking_parent);
  for (auto* layer : neg_z_order_list_)
    layer->SetStackingParent(stacking_parent);
}

#endif

bool PaintLayerStackingNode::StyleDidChange(PaintLayer& paint_layer,
                                            const ComputedStyle* old_style) {
  bool was_stacking_context = false;
  bool was_stacked = false;
  int old_z_index = 0;
  if (old_style) {
    was_stacking_context =
        paint_layer.GetLayoutObject().IsStackingContext(*old_style);
    old_z_index = old_style->EffectiveZIndex();
    was_stacked = paint_layer.GetLayoutObject().IsStacked(*old_style);
  }

  const ComputedStyle& new_style = paint_layer.GetLayoutObject().StyleRef();

  bool should_be_stacking_context =
      paint_layer.GetLayoutObject().IsStackingContext();
  bool should_be_stacked = paint_layer.GetLayoutObject().IsStacked();
  if (should_be_stacking_context == was_stacking_context &&
      was_stacked == should_be_stacked &&
      old_z_index == new_style.EffectiveZIndex())
    return false;

  // Need to force requirements update, due to change of stacking order.
  paint_layer.SetNeedsCompositingRequirementsUpdate();
  paint_layer.DirtyStackingContextZOrderLists();

  if (paint_layer.StackingNode())
    paint_layer.StackingNode()->DirtyZOrderLists();

  if (was_stacked != should_be_stacked) {
    if (!paint_layer.GetLayoutObject().DocumentBeingDestroyed() &&
        !paint_layer.IsRootLayer() && paint_layer.Compositor()) {
      paint_layer.Compositor()->SetNeedsCompositingUpdate(
          kCompositingUpdateRebuildTree);
    }
  }
  return true;
}

void PaintLayerStackingNode::UpdateZOrderLists() {
  if (z_order_lists_dirty_)
    RebuildZOrderLists();
}

}  // namespace blink
