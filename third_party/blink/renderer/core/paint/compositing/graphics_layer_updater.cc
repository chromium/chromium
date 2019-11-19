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

#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_updater.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

GraphicsLayerUpdater::UpdateContext::UpdateContext()
    : compositing_stacking_context_(nullptr),
      compositing_ancestor_(nullptr),
      use_slow_path_(false) {}

GraphicsLayerUpdater::UpdateContext::UpdateContext(const UpdateContext& other,
                                                   const PaintLayer& layer)
    : compositing_stacking_context_(other.compositing_stacking_context_),
      compositing_ancestor_(other.CompositingContainer(layer)),
      use_slow_path_(other.use_slow_path_) {
  CompositingState compositing_state = layer.GetCompositingState();
  if (compositing_state != kNotComposited &&
      compositing_state != kPaintsIntoGroupedBacking) {
    compositing_ancestor_ = &layer;
    if (layer.GetLayoutObject().StyleRef().IsStackingContext())
      compositing_stacking_context_ = &layer;
  }
  // Any composited content under SVG must be a descendant of (but not
  // equal to, see PaintLayerCompositor::CanBeComposited)
  // a <foreignObject> element. The rules for compositing ancestors are
  // complicated for this situation, due to <foreignObject> being a replaced
  // nornmal-flow stacking element
  // (see PaintLayer::IsReplacedNormalFlowStacking). Use a slow path
  // for these situations, to simplify the logic.
  if (layer.GetLayoutObject().IsSVGRoot() ||
      layer.IsReplacedNormalFlowStacking())
    use_slow_path_ = true;

  parent_object_offset_delta =
      compositing_ancestor_ == other.compositing_ancestor_
          ? other.parent_object_offset_delta
          : other.object_offset_delta;
}

const PaintLayer* GraphicsLayerUpdater::UpdateContext::CompositingContainer(
    const PaintLayer& layer) const {
  if (use_slow_path_)
    return layer.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf);

  const PaintLayer* compositing_container;
  if (layer.GetLayoutObject().StyleRef().IsStacked() &&
      !layer.IsReplacedNormalFlowStacking()) {
    compositing_container = compositing_stacking_context_;
  } else if ((layer.Parent() &&
              !layer.Parent()->GetLayoutObject().IsLayoutBlock()) ||
             layer.GetLayoutObject().IsColumnSpanAll()) {
    // In these cases, compositingContainer may escape the normal layer
    // hierarchy. Use the slow path to ensure correct result.
    // See PaintLayer::containingLayer() for details.
    compositing_container =
        layer.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf);
  } else {
    compositing_container = compositing_ancestor_;
  }

  // We should always get the same result as the slow path.
  DCHECK_EQ(compositing_container,
            layer.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  return compositing_container;
}

const PaintLayer*
GraphicsLayerUpdater::UpdateContext::CompositingStackingContext() const {
  return compositing_stacking_context_;
}

GraphicsLayerUpdater::GraphicsLayerUpdater() : needs_rebuild_tree_(false) {}

GraphicsLayerUpdater::~GraphicsLayerUpdater() = default;

void GraphicsLayerUpdater::Update(
    PaintLayer& layer,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  TRACE_EVENT0("blink", "GraphicsLayerUpdater::update");
  UpdateContext update_context;
  UpdateRecursive(layer, kDoNotForceUpdate, update_context,
                  layers_needing_paint_invalidation);
}

void GraphicsLayerUpdater::UpdateRecursive(
    PaintLayer& layer,
    UpdateType update_type,
    UpdateContext& context,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  if (layer.HasCompositedLayerMapping()) {
    CompositedLayerMapping* mapping = layer.GetCompositedLayerMapping();

    if (update_type == kForceUpdate || mapping->NeedsGraphicsLayerUpdate()) {
      bool had_scrolling_layer = mapping->ScrollingLayer();
      const auto* compositing_container = context.CompositingContainer(layer);
      if (mapping->UpdateGraphicsLayerConfiguration(compositing_container)) {
        needs_rebuild_tree_ = true;
        // Change of existence of scrolling layer affects visual rect offsets of
        // descendants via LayoutObject::ScrollAdjustmentForPaintInvalidation().
        if (had_scrolling_layer != !!mapping->ScrollingLayer())
          layers_needing_paint_invalidation.push_back(&layer);
      }
      mapping->UpdateGraphicsLayerGeometry(
          compositing_container, context.CompositingStackingContext(),
          layers_needing_paint_invalidation, context);
      if (PaintLayerScrollableArea* scrollable_area = layer.GetScrollableArea())
        scrollable_area->PositionOverflowControls();
      update_type = mapping->UpdateTypeForChildren(update_type);
      mapping->ClearNeedsGraphicsLayerUpdate();
    }
  }

  UpdateContext child_context(context, layer);
  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling()) {
    UpdateRecursive(*child, update_type, child_context,
                    layers_needing_paint_invalidation);
  }
}

#if DCHECK_IS_ON()

void GraphicsLayerUpdater::AssertNeedsToUpdateGraphicsLayerBitsCleared(
    PaintLayer& layer) {
  if (layer.HasCompositedLayerMapping()) {
    layer.GetCompositedLayerMapping()
        ->AssertNeedsToUpdateGraphicsLayerBitsCleared();
  }

  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling())
    AssertNeedsToUpdateGraphicsLayerBitsCleared(*child);
}

#endif

}  // namespace blink
