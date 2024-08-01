/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@dbaron.org>
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

#include "base/types/optional_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

// FIXME: This should not require PaintLayer. There is currently a cycle where
// in order to determine if we isStacked() we have to ask the paint
// layer about some of its state.
PaintLayerStackingNode::PaintLayerStackingNode(PaintLayer* layer)
    : layer_(layer) {
  DCHECK(layer->GetLayoutObject().IsStackingContext());
}

void PaintLayerStackingNode::DirtyZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_->LayerListMutationAllowed());
#endif

  pos_z_order_list_.clear();
  neg_z_order_list_.clear();

  for (auto& entry :
       layer_to_overlay_overflow_controls_painting_after_.Values()) {
    for (PaintLayer* layer : *entry)
      layer->SetNeedsReorderOverlayOverflowControls(false);
  }
  layer_to_overlay_overflow_controls_painting_after_.clear();

  z_order_lists_dirty_ = true;
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
  STACK_ALLOCATED();

 public:
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
      auto new_end = std::remove(highest_layers_order.begin(),
                                 highest_layers_order.end(), type);
      if (new_end != highest_layers_order.end()) {
        // |highest_layers_order| doesn't have duplicate elements, std::remove
        // will find at most one element at a time. So we don't shrink it and
        // just update the value of the |new_end|.
        DCHECK(std::next(new_end) == highest_layers_order.end());
        *new_end = type;
      } else {
        highest_layers_order.push_back(type);
      }
    }
  }

  static LayerType GetLayerType(const PaintLayer& layer) {
    DCHECK(layer.GetLayoutObject().IsStacked());
    const auto& style = layer.GetLayoutObject().StyleRef();
    if (style.GetPosition() == EPosition::kAbsolute)
      return kAbsolutePosition;
    if (style.GetPosition() == EPosition::kFixed)
      return kFixedPosition;
    return kInFlowStacked;
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

    UpdateOrderForSubtreeHighestLayers(GetLayerType(layer), &layer);
  }

  void Merge(HighestLayers& child, const PaintLayer& current_layer) {
    const auto& object = current_layer.GetLayoutObject();
    for (auto layer_type : child.highest_layers_order) {
      auto layer_type_for_propagation = layer_type;
      if (object.IsStacked()) {
        if ((layer_type == kAbsolutePosition &&
             object.CanContainAbsolutePositionObjects()) ||
            (layer_type == kFixedPosition &&
             object.CanContainFixedPositionObjects()) ||
            layer_type == kInFlowStacked) {
          // If the child is contained by the current layer, then use the
          // current layer's type for propagation to ancestors.
          layer_type_for_propagation = GetLayerType(current_layer);
        }
      }
      UpdateOrderForSubtreeHighestLayers(layer_type_for_propagation,
                                         child.highest_layers[layer_type]);
    }
  }
};

static LayoutObject* ChildOfFlexboxOrGridParentOrGrandparent(
    const PaintLayer* layer) {
  LayoutObject* parent = layer->GetLayoutObject().Parent();
  if (!parent) {
    return nullptr;
  }
  if (parent->IsFlexibleBox() || parent->IsLayoutGrid()) {
    return &layer->GetLayoutObject();
  }

  LayoutObject* grandparent = parent->Parent();
  if (!grandparent) {
    return nullptr;
  }
  if (grandparent->IsFlexibleBox() || grandparent->IsLayoutGrid()) {
    return parent;
  }
  return nullptr;
}

static bool OrderLessThan(const PaintLayer* first, const PaintLayer* second) {
  // TODO(chrishtr): make this work for arbitrary ancestors, not just parent
  // and grandparent.
  LayoutObject* first_ancestor = ChildOfFlexboxOrGridParentOrGrandparent(first);
  LayoutObject* second_ancestor =
      ChildOfFlexboxOrGridParentOrGrandparent(second);
  if (!first_ancestor || !second_ancestor) {
    return false;
  }

  if (first_ancestor->Parent() != second_ancestor->Parent()) {
    return false;
  }

  auto& first_style = first_ancestor->StyleRef();
  auto& second_style = second_ancestor->StyleRef();
  int first_order = 0;
  int second_order = 0;
  // Out of flow flexbox direct children paint as if order was 0:
  // https://drafts.csswg.org/css-display-4/#order-modified-document-order
  if (first_ancestor != first->GetLayoutObject() ||
      !first_ancestor->IsOutOfFlowPositioned()) {
    first_order = first_style.Order();
  }
  if (second_ancestor != second->GetLayoutObject() ||
      !second_ancestor->IsOutOfFlowPositioned()) {
    second_order = second_style.Order();
  }
  return first_order < second_order;
}

// Returns the children of |paint_layer|, sorted by the order CSS property
// if they are the child of a flexbox. See:
// https://www.w3.org/TR/css-flexbox-1/#painting
static void GetOrderSortedChildren(
    PaintLayer* paint_layer,
    PaintLayerStackingNode::PaintLayers& sorted_children) {
  for (PaintLayer* child = paint_layer->FirstChild(); child;
       child = child->NextSibling()) {
    sorted_children.push_back(child);
  }

  std::stable_sort(sorted_children.begin(), sorted_children.end(),
                   OrderLessThan);
}

void PaintLayerStackingNode::RebuildZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_->LayerListMutationAllowed());
#endif
  DCHECK(z_order_lists_dirty_);

  layer_->SetNeedsReorderOverlayOverflowControls(false);
  PaintLayers order_sorted_children;
  GetOrderSortedChildren(layer_, order_sorted_children);
  for (auto& child : order_sorted_children) {
    CollectLayers(*child, nullptr);
  }

  // Sort the two lists.
  std::stable_sort(pos_z_order_list_.begin(), pos_z_order_list_.end(),
                   ZIndexLessThan);
  std::stable_sort(neg_z_order_list_.begin(), neg_z_order_list_.end(),
                   ZIndexLessThan);

  // Append layers for top layer elements after normal layer collection, to
  // ensure they are on top regardless of z-indexes.  The layoutObjects of top
  // layer elements are children of the view, sorted in top layer stacking
  // order.
  if (layer_->IsRootLayer()) {
    LayoutBlockFlow* root_block = layer_->GetLayoutObject().View();
    // If the viewport is paginated, everything (including "top-layer" elements)
    // gets redirected to the flow thread. So that's where we have to look, in
    // that case.
    if (LayoutBlockFlow* multi_column_flow_thread =
            root_block->MultiColumnFlowThread())
      root_block = multi_column_flow_thread;
    for (LayoutObject* child = root_block->FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsInTopOrViewTransitionLayer() && child->IsStacked()) {
        pos_z_order_list_.push_back(To<LayoutBoxModelObject>(child)->Layer());
      }
    }
  }
  z_order_lists_dirty_ = false;
}

void PaintLayerStackingNode::CollectLayers(PaintLayer& paint_layer,
                                           HighestLayers* highest_layers) {
  paint_layer.SetNeedsReorderOverlayOverflowControls(false);

  if (paint_layer.IsInTopOrViewTransitionLayer()) {
    return;
  }

  if (highest_layers)
    highest_layers->Update(paint_layer);

  const auto& object = paint_layer.GetLayoutObject();
  const auto& style = object.StyleRef();

  if (object.IsStacked()) {
    auto& list =
        style.EffectiveZIndex() >= 0 ? pos_z_order_list_ : neg_z_order_list_;
    list.push_back(paint_layer);
  }

  if (object.IsStackingContext())
    return;

  std::optional<HighestLayers> subtree_highest_layers;
  bool has_overlay_overflow_controls =
      paint_layer.GetScrollableArea() &&
      paint_layer.GetScrollableArea()->HasOverlayOverflowControls();
  if (has_overlay_overflow_controls || highest_layers)
    subtree_highest_layers.emplace();

  PaintLayers order_sorted_children;
  GetOrderSortedChildren(&paint_layer, order_sorted_children);
  for (auto& child : order_sorted_children) {
    CollectLayers(*child, base::OptionalToPtr(subtree_highest_layers));
  }

  if (has_overlay_overflow_controls) {
    DCHECK(subtree_highest_layers);
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
          .insert(layer_to_paint_overlay_overflow_controls_after,
                  MakeGarbageCollected<PaintLayers>())
          .stored_value->value->push_back(paint_layer);
    }
    paint_layer.SetNeedsReorderOverlayOverflowControls(
        !!layer_to_paint_overlay_overflow_controls_after);
  }

  if (highest_layers)
    highest_layers->Merge(*subtree_highest_layers, paint_layer);
}

bool PaintLayerStackingNode::StyleDidChange(PaintLayer& paint_layer,
                                            const ComputedStyle* old_style) {
  bool was_stacking_context = false;
  bool was_stacked = false;
  int old_z_index = 0;
  int old_order = 0;
  if (old_style) {
    was_stacking_context =
        paint_layer.GetLayoutObject().IsStackingContext(*old_style);
    old_z_index = old_style->EffectiveZIndex();
    old_order = old_style->Order();
    was_stacked = paint_layer.GetLayoutObject().IsStacked(*old_style);
  }

  const ComputedStyle& new_style = paint_layer.GetLayoutObject().StyleRef();

  bool should_be_stacking_context =
      paint_layer.GetLayoutObject().IsStackingContext();
  bool should_be_stacked = paint_layer.GetLayoutObject().IsStacked();
  if (should_be_stacking_context == was_stacking_context &&
      was_stacked == should_be_stacked &&
      old_z_index == new_style.EffectiveZIndex() &&
      old_order == new_style.Order()) {
    return false;
  }

  paint_layer.DirtyStackingContextZOrderLists();

  if (paint_layer.StackingNode())
    paint_layer.StackingNode()->DirtyZOrderLists();
  return true;
}

void PaintLayerStackingNode::UpdateZOrderLists() {
  if (z_order_lists_dirty_)
    RebuildZOrderLists();
}

void PaintLayerStackingNode::Trace(Visitor* visitor) const {
  visitor->Trace(layer_);
  visitor->Trace(pos_z_order_list_);
  visitor->Trace(neg_z_order_list_);
  visitor->Trace(layer_to_overlay_overflow_controls_painting_after_);
}

}  // namespace blink
