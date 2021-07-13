// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

void SetLayerNeedsRepaintOnCullRectChange(PaintLayer& layer) {
  if (layer.PreviousPaintResult() == kMayBeClippedByCullRect ||
      RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    layer.SetNeedsRepaint();
  }
}

void SetFragmentCullRect(PaintLayer& layer,
                         FragmentData& fragment,
                         const CullRect& cull_rect) {
  if (cull_rect == fragment.GetCullRect())
    return;

  fragment.SetCullRect(cull_rect);
  SetLayerNeedsRepaintOnCullRectChange(layer);
}

// Returns true if the contents cull rect changed.
bool SetFragmentContentsCullRect(PaintLayer& layer,
                                 FragmentData& fragment,
                                 const CullRect& contents_cull_rect) {
  if (contents_cull_rect == fragment.GetContentsCullRect())
    return false;

  fragment.SetContentsCullRect(contents_cull_rect);
  SetLayerNeedsRepaintOnCullRectChange(layer);
  return true;
}

}  // anonymous namespace

void CullRectUpdater::Update() {
  DCHECK(starting_layer_.IsRootLayer());
  UpdateInternal(CullRect::Infinite());
#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "PaintLayer tree after cull rect update:";
    showLayerTree(&starting_layer_);
  }
#endif
}

void CullRectUpdater::UpdateInternal(const CullRect& input_cull_rect) {
  DCHECK(RuntimeEnabledFeatures::CullRectUpdateEnabled());
  const auto& object = starting_layer_.GetLayoutObject();
  if (object.GetFrameView()->ShouldThrottleRendering())
    return;

  root_state_ =
      object.View()->FirstFragment().LocalBorderBoxProperties().Unalias();
  bool should_use_infinite =
      PaintLayerPainter(starting_layer_).ShouldUseInfiniteCullRect();
  auto& fragment = object.GetMutableForPainting().FirstFragment();
  SetFragmentCullRect(
      starting_layer_, fragment,
      should_use_infinite ? CullRect::Infinite() : input_cull_rect);
  bool force_update_children = SetFragmentContentsCullRect(
      starting_layer_, fragment,
      should_use_infinite ? CullRect::Infinite()
                          : ComputeFragmentContentsCullRect(
                                starting_layer_, fragment, input_cull_rect));
  UpdateForDescendants(starting_layer_, force_update_children);
}

void CullRectUpdater::UpdateRecursively(PaintLayer& layer,
                                        const PaintLayer& parent_painting_layer,
                                        bool force_update_self) {
  bool should_proactively_update = ShouldProactivelyUpdate(layer);
  bool force_update_children =
      should_proactively_update || layer.ForcesChildrenCullRectUpdate();

  // This defines the scope of force_proactive_update_ (which may be set by
  // ComputeFragmentCullRect() and ComputeFragmentContentsCullRect()) to the
  // subtree.
  base::AutoReset<bool> reset(&force_proactive_update_,
                              force_proactive_update_);

  if (force_update_self || should_proactively_update ||
      layer.NeedsCullRectUpdate())
    force_update_children |= UpdateForSelf(layer, parent_painting_layer);

  if (force_update_children || layer.DescendantNeedsCullRectUpdate())
    UpdateForDescendants(layer, force_update_children);

  layer.ClearNeedsCullRectUpdate();
}

void CullRectUpdater::UpdateForDescendants(PaintLayer& layer,
                                           bool force_update_children) {
  const auto& object = layer.GetLayoutObject();

  // DisplayLockContext will force cull rect update of the subtree on unlock.
  if (object.ChildPaintBlockedByDisplayLock())
    return;

  if (auto* embedded_content = DynamicTo<LayoutEmbeddedContent>(object)) {
    if (auto* embedded_view = embedded_content->GetEmbeddedContentView()) {
      if (auto* embedded_frame_view =
              DynamicTo<LocalFrameView>(embedded_view)) {
        PaintLayer* subframe_root_layer = nullptr;
        if (auto* sub_layout_view = embedded_frame_view->GetLayoutView())
          subframe_root_layer = sub_layout_view->Layer();
        if (embedded_frame_view->ShouldThrottleRendering()) {
          if (force_update_children && subframe_root_layer)
            subframe_root_layer->SetNeedsCullRectUpdate();
        } else {
          DCHECK(subframe_root_layer);
          UpdateRecursively(*subframe_root_layer, layer, force_update_children);
        }
      }
    }
  }

  // Update non-stacked direct children first. In the following case:
  // <div id="layer" style="stacking-context">
  //   <div id="child" style="overflow: hidden; ...">
  //     <div id="stacked-child" style="position: relative"></div>
  //   </div>
  // </div>
  // If |child|'s contents cull rect changes, we need to update |stack-child|'s
  // cull rect because it's clipped by |child|. The is done in the following
  // order:
  //   UpdateForDescendants(|layer|)
  //     UpdateRecursively(|child|) (in the following loop)
  //       |stacked-child|->SetNeedsCullRectUpdate()
  //     UpdateRecursively(stacked-child) (in the next loop)
  // Note that this iterates direct children (including non-stacked, and
  // stacked children which may not be paint-order children of |layer|, e.g.
  // |stacked-child| is not a paint-order child of |child|), which is
  // different from PaintLayerPaintOrderIterator(kAllChildren) which iterates
  // children in paint order.
  for (auto* child = layer.FirstChild(); child; child = child->NextSibling()) {
    if (child->GetLayoutObject().IsStacked()) {
      // In the above example, during UpdateForDescendants(child), this
      // forces cull rect update of |stacked-child| which will be updated in
      // the next loop during UpdateForDescendants(layer).
      if (force_update_children)
        child->SetNeedsCullRectUpdate();
      continue;
    }
    UpdateRecursively(*child, layer, force_update_children);
  }

  // Then stacked children (which may not be direct children in PaintLayer
  // hierarchy) in paint order.
  PaintLayerPaintOrderIterator iterator(layer, kStackedChildren);
  while (PaintLayer* child = iterator.Next())
    UpdateRecursively(*child, layer, force_update_children);
}

bool CullRectUpdater::UpdateForSelf(PaintLayer& layer,
                                    const PaintLayer& parent_painting_layer) {
  const auto& first_parent_fragment =
      parent_painting_layer.GetLayoutObject().FirstFragment();
  auto& first_fragment =
      layer.GetLayoutObject().GetMutableForPainting().FirstFragment();
  // If both |layer| and |parent_painting_layer| are fragmented and are inside
  // the same pagination container, then try to match fragments from
  // |parent_painting_layer| to |layer|, so that any fragment clip for
  // |parent_painting_layer|'s fragment matches |layer|'s. Note we check both
  // ShouldFragmentCompositedBounds() and next fragment here because the former
  // may return false even if |layer| is fragmented, e.g. for fixed-position
  // objects in paged media, and the next fragment can be null even if the first
  // fragment is actually in a fragmented context when the current layer appears
  // in only one of the multiple fragments of the pagination container.
  bool is_fragmented =
      layer.ShouldFragmentCompositedBounds() || first_fragment.NextFragment();
  bool should_match_fragments =
      is_fragmented && parent_painting_layer.EnclosingPaginationLayer() ==
                           layer.EnclosingPaginationLayer();
  bool force_update_children = false;

  for (auto* fragment = &first_fragment; fragment;
       fragment = fragment->NextFragment()) {
    const FragmentData* parent_fragment = nullptr;
    if (should_match_fragments) {
      for (parent_fragment = &first_parent_fragment; parent_fragment;
           parent_fragment = parent_fragment->NextFragment()) {
        if (parent_fragment->FragmentID() == fragment->FragmentID())
          break;
      }
    } else {
      parent_fragment = &first_parent_fragment;
    }

    CullRect cull_rect;
    CullRect contents_cull_rect;
    if (!parent_fragment ||
        PaintLayerPainter(layer).ShouldUseInfiniteCullRect()) {
      cull_rect = CullRect::Infinite();
      contents_cull_rect = CullRect::Infinite();
    } else {
      cull_rect = ComputeFragmentCullRect(layer, *fragment, *parent_fragment);
      contents_cull_rect =
          ComputeFragmentContentsCullRect(layer, *fragment, cull_rect);
    }

    SetFragmentCullRect(layer, *fragment, cull_rect);
    force_update_children |=
        SetFragmentContentsCullRect(layer, *fragment, contents_cull_rect);
  }

  return force_update_children;
}

CullRect CullRectUpdater::ComputeFragmentCullRect(
    PaintLayer& layer,
    const FragmentData& fragment,
    const FragmentData& parent_fragment) {
  CullRect cull_rect = parent_fragment.GetContentsCullRect();
  auto parent_state = parent_fragment.ContentsProperties().Unalias();
  auto local_state = fragment.LocalBorderBoxProperties().Unalias();
  if (parent_state != local_state) {
    absl::optional<CullRect> old_cull_rect;
    // Not using |old_cull_rect| will force the cull rect to be updated
    // (skipping |ChangedEnough|) in |ApplyPaintProperties|.
    if (!ShouldProactivelyUpdate(layer))
      old_cull_rect = fragment.GetCullRect();
    bool expanded = cull_rect.ApplyPaintProperties(root_state_, parent_state,
                                                   local_state, old_cull_rect);
    if (expanded && fragment.GetCullRect() != cull_rect)
      force_proactive_update_ = true;
  }
  return cull_rect;
}

CullRect CullRectUpdater::ComputeFragmentContentsCullRect(
    PaintLayer& layer,
    const FragmentData& fragment,
    const CullRect& cull_rect) {
  auto local_state = fragment.LocalBorderBoxProperties().Unalias();
  CullRect contents_cull_rect = cull_rect;
  auto contents_state = fragment.ContentsProperties().Unalias();
  if (contents_state != local_state) {
    absl::optional<CullRect> old_contents_cull_rect;
    // Not using |old_cull_rect| will force the cull rect to be updated
    // (skipping |CullRect::ChangedEnough|) in |ApplyPaintProperties|.
    if (!ShouldProactivelyUpdate(layer))
      old_contents_cull_rect = fragment.GetContentsCullRect();
    bool expanded = contents_cull_rect.ApplyPaintProperties(
        root_state_, local_state, contents_state, old_contents_cull_rect);
    if (expanded && fragment.GetContentsCullRect() != contents_cull_rect)
      force_proactive_update_ = true;
  }
  return contents_cull_rect;
}

bool CullRectUpdater::ShouldProactivelyUpdate(const PaintLayer& layer) const {
  if (force_proactive_update_)
    return true;

  // If we will repaint anyway, proactively refresh cull rect. A sliding
  // window (aka hysteresis, see: CullRect::ChangedEnough()) is used to
  // avoid frequent cull rect updates because they force a repaint (see:
  // |CullRectUpdater::SetFragmentCullRects|). Proactively updating the cull
  // rect resets the sliding window which will minimize the need to update
  // the cull rect again.
  return layer.SelfOrDescendantNeedsRepaint();
}

OverriddenCullRectScope::OverriddenCullRectScope(PaintLayer& starting_layer,
                                                 const CullRect& cull_rect)
    : starting_layer_(starting_layer) {
  if (!RuntimeEnabledFeatures::CullRectUpdateEnabled())
    return;

  if (starting_layer.GetLayoutObject().GetFrame()->IsLocalRoot() &&
      !starting_layer.NeedsCullRectUpdate() &&
      !starting_layer.DescendantNeedsCullRectUpdate() &&
      cull_rect ==
          starting_layer.GetLayoutObject().FirstFragment().GetCullRect()) {
    // The current cull rects are good.
    return;
  }

  updated_ = true;
  starting_layer.SetNeedsCullRectUpdate();
  CullRectUpdater(starting_layer).UpdateInternal(cull_rect);
}

OverriddenCullRectScope::~OverriddenCullRectScope() {
  if (RuntimeEnabledFeatures::CullRectUpdateEnabled() && updated_)
    starting_layer_.SetNeedsCullRectUpdate();
}

}  // namespace blink
