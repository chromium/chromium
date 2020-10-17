/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_builder.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"

namespace blink {

GraphicsLayerTreeBuilder::GraphicsLayerTreeBuilder() = default;

static bool ShouldAppendLayer(const PaintLayer& layer) {
  auto* video_element =
      DynamicTo<HTMLVideoElement>(layer.GetLayoutObject().GetNode());
  if (video_element && video_element->IsFullscreen() &&
      video_element->UsesOverlayFullscreenVideo()) {
    return false;
  }
  return true;
}

void GraphicsLayerTreeBuilder::Rebuild(PaintLayer& layer,
                                       GraphicsLayerVector& child_layers) {
  PendingOverflowControlReparents ignored;
  RebuildRecursive(layer, child_layers, ignored);
}

using PendingPair = std::pair<const PaintLayer*, wtf_size_t>;

void GraphicsLayerTreeBuilder::RebuildRecursive(
    PaintLayer& layer,
    GraphicsLayerVector& child_layers,
    PendingOverflowControlReparents& pending_reparents) {
  // Make the layer compositing if necessary, and set up clipping and content
  // layers.  Note that we can only do work here that is independent of whether
  // the descendant layers have been processed. computeCompositingRequirements()
  // will already have done the paint invalidation if necessary.

  const bool has_composited_layer_mapping = layer.HasCompositedLayerMapping();
  CompositedLayerMapping* current_composited_layer_mapping =
      layer.GetCompositedLayerMapping();

  // If this layer has a compositedLayerMapping, then that is where we place
  // subsequent children GraphicsLayers.  Otherwise children continue to append
  // to the child list of the enclosing layer.
  GraphicsLayerVector this_layer_children;
  GraphicsLayerVector* layer_vector_for_children =
      has_composited_layer_mapping ? &this_layer_children : &child_layers;

  PendingOverflowControlReparents this_pending_reparents_children;
  PendingOverflowControlReparents* pending_reparents_for_children =
      has_composited_layer_mapping ? &this_pending_reparents_children
                                   : &pending_reparents;

#if DCHECK_IS_ON()
  PaintLayerListMutationDetector mutation_checker(layer);
#endif

  bool recursion_blocked_by_display_lock =
      layer.GetLayoutObject().ChildPrePaintBlockedByDisplayLock();
  // If the recursion is blocked meaningfully (i.e. we would have recursed,
  // since the layer has children), then we should inform the display-lock
  // context that we blocked a graphics layer recursion, so that we can ensure
  // to rebuild the tree once we're unlocked.
  if (recursion_blocked_by_display_lock && layer.FirstChild()) {
    auto* context = layer.GetLayoutObject().GetDisplayLockContext();
    DCHECK(context);
    context->NotifyGraphicsLayerRebuildBlocked();
  }

  if (layer.IsStackingContextWithNegativeZOrderChildren()) {
    if (!recursion_blocked_by_display_lock) {
      PaintLayerPaintOrderIterator iterator(layer, kNegativeZOrderChildren);
      while (PaintLayer* child_layer = iterator.Next()) {
        RebuildRecursive(*child_layer, *layer_vector_for_children,
                         *pending_reparents_for_children);
      }
    }

    // If a negative z-order child is compositing, we get a foreground layer
    // which needs to get parented.
    if (has_composited_layer_mapping &&
        current_composited_layer_mapping->ForegroundLayer()) {
      layer_vector_for_children->push_back(
          current_composited_layer_mapping->ForegroundLayer());
    }
  }

  if (!recursion_blocked_by_display_lock) {
    PaintLayerPaintOrderIterator iterator(layer,
                                          kNormalFlowAndPositiveZOrderChildren);
    while (PaintLayer* child_layer = iterator.Next()) {
      RebuildRecursive(*child_layer, *layer_vector_for_children,
                       *pending_reparents_for_children);
    }
  }

  if (layer.GetLayoutObject().IsLayoutEmbeddedContent()) {
    DCHECK(this_layer_children.IsEmpty());
    PaintLayerCompositor* inner_compositor =
        PaintLayerCompositor::FrameContentsCompositor(
            ToLayoutEmbeddedContent(layer.GetLayoutObject()));
    if (inner_compositor &&
        inner_compositor->CanBeComposited(inner_compositor->RootLayer())) {
      if (inner_compositor->InCompositingMode()) {
        if (GraphicsLayer* inner_root_layer =
                inner_compositor->RootGraphicsLayer()) {
          layer_vector_for_children->push_back(inner_root_layer);
        }
      }
      inner_compositor->ClearRootLayerAttachmentDirty();
    }
  }

  if (has_composited_layer_mapping) {
    // TODO(szager): Remove after diagnosing crash crbug.com/1092673
    CHECK(current_composited_layer_mapping);

    // Apply all pending reparents by inserting the overflow controls
    // root layers into |this_layer_children|. To do this, first sort
    // them by index. Then insert them one-by-one into the array,
    // incrementing |offset| by one each time to account for previous
    // insertions.
    wtf_size_t offset = 0;
    Vector<PendingPair> pending;
    for (auto& item : *pending_reparents_for_children)
      pending.push_back(std::make_pair(item.key, item.value));
    pending_reparents_for_children->clear();

    std::sort(pending.begin(), pending.end(),
              [](const PendingPair& a, const PendingPair& b) {
                return a.second < b.second;
              });
    for (auto& item : pending) {
      offset += item.first->GetCompositedLayerMapping()
                    ->MoveOverflowControlLayersInto(this_layer_children,
                                                    item.second + offset);
    }

    if (!this_layer_children.IsEmpty()) {
      current_composited_layer_mapping->SetSublayers(
          std::move(this_layer_children));
    }

    if (ShouldAppendLayer(layer)) {
      child_layers.push_back(
          current_composited_layer_mapping->MainGraphicsLayer());
    }
  }

  // Also insert for self, to handle the case of scrollers with negative
  // z-index children (the scrolbars should still paint on top of the
  // scroller itself).
  if (layer.GetLayoutObject().IsStacked() && has_composited_layer_mapping &&
      layer.GetCompositedLayerMapping()->NeedsToReparentOverflowControls())
    pending_reparents.Set(&layer, child_layers.size());

  // Set or overwrite the entry in |pending_reparents| for this scroller.
  // Overlay scrollbars need to paint on top of all content under the scroller,
  // so keep overwriting if we find a PaintLayer that is later in paint order.
  const PaintLayer* scroll_parent = layer.ScrollParent();
  if (layer.GetLayoutObject().IsStacked() && scroll_parent &&
      scroll_parent->HasCompositedLayerMapping() &&
      scroll_parent->GetCompositedLayerMapping()
          ->NeedsToReparentOverflowControls())
    pending_reparents.Set(layer.ScrollParent(), child_layers.size());
}

}  // namespace blink
