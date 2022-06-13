// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

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

bool ShouldUseInfiniteCullRect(const PaintLayer& layer,
                               bool& subtree_should_use_infinite_cull_rect) {
  if (RuntimeEnabledFeatures::InfiniteCullRectEnabled())
    return true;

  if (subtree_should_use_infinite_cull_rect)
    return true;

  const LayoutObject& object = layer.GetLayoutObject();
  bool is_printing = object.GetDocument().Printing();
  if (IsA<LayoutView>(object) && !object.GetFrame()->ClipsContent() &&
      // We use custom top cull rect per page when printing.
      !is_printing) {
    return true;
  }

  if (const auto* properties = object.FirstFragment().PaintProperties()) {
    // Cull rects and clips can't be propagated across a filter which moves
    // pixels, since the input of the filter may be outside the cull rect /
    // clips yet still result in painted output.
    if (properties->Filter() &&
        properties->Filter()->HasFilterThatMovesPixels() &&
        // However during printing, we don't want filter outset to cross page
        // boundaries. This also avoids performance issue because the PDF
        // renderer is super slow for big filters.
        !is_printing) {
      return true;
    }

    // Cull rect mapping doesn't work under perspective in some cases.
    // See http://crbug.com/887558 for details.
    if (properties->Perspective()) {
      subtree_should_use_infinite_cull_rect = true;
      return true;
    }

    const TransformPaintPropertyNode* transform_nodes[] = {
        properties->Transform(), properties->Offset(), properties->Scale(),
        properties->Rotate(), properties->Translate()};
    for (const auto* transform : transform_nodes) {
      if (!transform)
        continue;

      // A CSS transform can also have perspective like
      // "transform: perspective(100px) rotateY(45deg)". In these cases, we
      // also want to skip cull rect mapping. See http://crbug.com/887558 for
      // details.
      if (!transform->IsIdentityOr2DTranslation() &&
          transform->Matrix().HasPerspective()) {
        subtree_should_use_infinite_cull_rect = true;
        return true;
      }

      // Ensure content under animating transforms is not culled out.
      if (transform->HasActiveTransformAnimation())
        return true;

      // As an optimization, skip cull rect updating for non-composited
      // transforms which have already been painted. This is because the cull
      // rect update, which needs to do complex mapping of the cull rect, can
      // be more expensive than over-painting.
      if (!transform->HasDirectCompositingReasons() &&
          layer.PreviousPaintResult() == kFullyPainted) {
        return true;
      }
    }
  }

  return false;
}

}  // anonymous namespace

void CullRectUpdater::Update() {
  TRACE_EVENT0("blink,benchmark", "CullRectUpdate");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Blink.CullRect.UpdateTime");

  DCHECK(starting_layer_.IsRootLayer());
  UpdateInternal(CullRect::Infinite());
#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "PaintLayer tree after cull rect update:";
    ShowLayerTree(&starting_layer_);
  }
#endif
}

void CullRectUpdater::UpdateInternal(const CullRect& input_cull_rect) {
  const auto& object = starting_layer_.GetLayoutObject();
  if (object.GetFrameView()->ShouldThrottleRendering())
    return;

  root_state_ =
      object.View()->FirstFragment().LocalBorderBoxProperties().Unalias();
  bool should_use_infinite = ShouldUseInfiniteCullRect(
      starting_layer_, subtree_should_use_infinite_cull_rect_);
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

// See UpdateForDescendants for how |force_update_self| is propagated.
void CullRectUpdater::UpdateRecursively(PaintLayer& layer,
                                        const PaintLayer& parent_painting_layer,
                                        bool force_update_self) {
  if (layer.IsUnderSVGHiddenContainer())
    return;

  bool should_proactively_update = ShouldProactivelyUpdate(layer);
  bool force_update_children =
      should_proactively_update || layer.ForcesChildrenCullRectUpdate() ||
      !layer.GetLayoutObject().IsStackingContext() ||
      // |force_update_self| is true if the contents cull rect of the containing
      // block of |layer| changed, so we need to propagate the flag to
      // non-contained absolute-position descendants whose cull rects are also
      // affected by the containing block.
      (force_update_self && layer.HasNonContainedAbsolutePositionDescendant());

  // This defines the scope of force_proactive_update_ (which may be set by
  // ComputeFragmentCullRect() and ComputeFragmentContentsCullRect()) to the
  // subtree.
  base::AutoReset<bool> reset_force_update(&force_proactive_update_,
                                           force_proactive_update_);
  // Similarly for subtree_should_use_infinite_cull_rect_.
  base::AutoReset<bool> reset_subtree_infinite_cull_rect(
      &subtree_should_use_infinite_cull_rect_,
      subtree_should_use_infinite_cull_rect_);

  if (force_update_self || should_proactively_update ||
      layer.NeedsCullRectUpdate())
    force_update_children |= UpdateForSelf(layer, parent_painting_layer);

  absl::optional<base::AutoReset<bool>> reset_subtree_is_out_of_cull_rect;
  if (!subtree_is_out_of_cull_rect_ && layer.KnownToClipSubtree() &&
      !layer.GetLayoutObject().FirstFragment().NextFragment()) {
    const auto* box = layer.GetLayoutBox();
    DCHECK(box);
    PhysicalRect overflow_rect = box->PhysicalSelfVisualOverflowRect();
    overflow_rect.Move(box->FirstFragment().PaintOffset());
    if (!box->FirstFragment().GetCullRect().Intersects(
            ToEnclosingRect(overflow_rect))) {
      reset_subtree_is_out_of_cull_rect.emplace(&subtree_is_out_of_cull_rect_,
                                                true);
    }
  }

  if (force_update_children || layer.DescendantNeedsCullRectUpdate() ||
      // A change of non-stacking-context layer may affect cull rect of
      // descendants even if the contents cull rect doesn't change.
      !layer.GetLayoutObject().IsStackingContext()) {
    UpdateForDescendants(layer, force_update_children);
  }

  layer.ClearNeedsCullRectUpdate();
}

// "Children" in |force_update_children| means children in the containing block
// tree. The flag is set by the containing block whose contents cull rect
// changed.
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
  // If |child| needs cull rect update, we also need to update |stacked-child|'s
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
    if (!child->IsReplacedNormalFlowStacking() &&
        child->GetLayoutObject().IsStacked()) {
      // In the above example, during UpdateForDescendants(child), this
      // forces cull rect update of |stacked-child| which will be updated in
      // the next loop during UpdateForDescendants(layer).
      child->SetNeedsCullRectUpdate();
      continue;
    }
    UpdateRecursively(*child, layer, force_update_children);
  }

  // Then stacked children (which may not be direct children in PaintLayer
  // hierarchy) in paint order.
  PaintLayerPaintOrderIterator iterator(&layer, kStackedChildren);
  while (PaintLayer* child = iterator.Next()) {
    if (!child->IsReplacedNormalFlowStacking())
      UpdateRecursively(*child, layer, force_update_children);
  }
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
  // EnclosingPaginationLayer() and next fragment here because the former
  // may return false even if |layer| is fragmented, e.g. for fixed-position
  // objects in paged media, and the next fragment can be null even if the first
  // fragment is actually in a fragmented context when the current layer appears
  // in only one of the multiple fragments of the pagination container.
  bool is_fragmented =
      layer.EnclosingPaginationLayer() || first_fragment.NextFragment();
  bool should_match_fragments =
      is_fragmented && parent_painting_layer.EnclosingPaginationLayer() ==
                           layer.EnclosingPaginationLayer();
  bool force_update_children = false;
  bool should_use_infinite_cull_rect =
      !subtree_is_out_of_cull_rect_ &&
      ShouldUseInfiniteCullRect(layer, subtree_should_use_infinite_cull_rect_);

  for (auto* fragment = &first_fragment; fragment;
       fragment = fragment->NextFragment()) {
    CullRect cull_rect;
    CullRect contents_cull_rect;
    if (subtree_is_out_of_cull_rect_) {
      // PaintLayerPainter may skip the subtree including this layer, so we
      // need to SetPreviousPaintResult() here.
      layer.SetPreviousPaintResult(kMayBeClippedByCullRect);
    } else {
      const FragmentData* parent_fragment = nullptr;
      if (!should_use_infinite_cull_rect) {
        if (should_match_fragments) {
          for (parent_fragment = &first_parent_fragment; parent_fragment;
               parent_fragment = parent_fragment->NextFragment()) {
            if (parent_fragment->FragmentID() == fragment->FragmentID())
              break;
          }
        } else {
          parent_fragment = &first_parent_fragment;
        }
      }

      if (should_use_infinite_cull_rect || !parent_fragment) {
        cull_rect = CullRect::Infinite();
        contents_cull_rect = CullRect::Infinite();
      } else {
        cull_rect = ComputeFragmentCullRect(layer, *fragment, *parent_fragment);
        contents_cull_rect =
            ComputeFragmentContentsCullRect(layer, *fragment, cull_rect);
      }
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
  auto local_state = fragment.LocalBorderBoxProperties().Unalias();
  CullRect cull_rect = parent_fragment.GetContentsCullRect();
  auto parent_state = parent_fragment.ContentsProperties().Unalias();

  if (layer.GetLayoutObject().IsFixedPositioned()) {
    const auto& view_fragment = layer.GetLayoutObject().View()->FirstFragment();
    auto view_state = view_fragment.LocalBorderBoxProperties().Unalias();
    if (const auto* properties = fragment.PaintProperties()) {
      if (const auto* translation = properties->PaintOffsetTranslation()) {
        if (translation->Parent() == &view_state.Transform()) {
          // Use the viewport clip and ignore additional clips (e.g. clip-paths)
          // because they are applied on this fixed-position layer by
          // non-containers which may change location relative to this layer on
          // viewport scroll for which we don't want to change fixed-position
          // cull rects for performance.
          local_state.SetClip(
              view_fragment.ContentsProperties().Clip().Unalias());
          parent_state = view_state;
          cull_rect = view_fragment.GetCullRect();
        }
      }
    }
  }

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

void CullRectUpdater::PaintPropertiesChanged(
    const LayoutObject& object,
    PaintLayer& painting_layer,
    const PaintPropertiesChangeInfo& properties_changed,
    const gfx::Vector2dF& old_scroll_offset) {
  // We don't need to update cull rect for kChangedOnlyCompositedValues (except
  // for some paint translation changes, see below) because we expect no repaint
  // or PAC update for performance.
  // Clip nodes and scroll nodes don't have kChangedOnlyCompositedValues, so we
  // don't need to check ShouldUseInfiniteCullRect before the early return
  // below.
  DCHECK_NE(properties_changed.clip_changed,
            PaintPropertyChangeType::kChangedOnlyCompositedValues);
  DCHECK_NE(properties_changed.scroll_changed,
            PaintPropertyChangeType::kChangedOnlyCompositedValues);
  // Cull rects depend on transforms, clip rects and scroll contents sizes.
  bool needs_cull_rect_update =
      properties_changed.transform_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues ||
      properties_changed.clip_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues ||
      properties_changed.scroll_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues;

  if (!needs_cull_rect_update) {
    if (const auto* properties = object.FirstFragment().PaintProperties()) {
      if (const auto* scroll_translation = properties->ScrollTranslation()) {
        // TODO(wangxianzhu): We can avoid cull rect update on scroll
        // - if the scroll delta is not big enough to cause cull rect update, or
        // - if the current contents cull rect is infinite and no descendants
        //   need cull rect update.
        needs_cull_rect_update =
            scroll_translation->Translation2D() != old_scroll_offset;
      }
    }
  }

  if (!needs_cull_rect_update) {
    // For cases that the transform change can be directly updated, we should
    // use infinite cull rect to avoid cull rect change and repaint.
    bool subtree_should_use_infinite_cull_rect = false;
    DCHECK(properties_changed.transform_changed !=
               PaintPropertyChangeType::kChangedOnlyCompositedValues ||
           object.IsSVGChild() ||
           ShouldUseInfiniteCullRect(painting_layer,
                                     subtree_should_use_infinite_cull_rect));
    return;
  }

  if (object.HasLayer()) {
    To<LayoutBoxModelObject>(object).Layer()->SetNeedsCullRectUpdate();
    if (object.IsLayoutView() &&
        object.GetFrameView()->HasViewportConstrainedObjects()) {
      // Fixed-position cull rects depend on view clip. See
      // ComputeFragmentCullRect().
      if (const auto* clip_node =
              object.FirstFragment().PaintProperties()->OverflowClip()) {
        if (clip_node->NodeChanged() != PaintPropertyChangeType::kUnchanged) {
          for (auto constrained :
               *object.GetFrameView()->ViewportConstrainedObjects()) {
            if (constrained->IsFixedPositioned()) {
              To<LayoutBoxModelObject>(constrained.Get())
                  ->Layer()
                  ->SetNeedsCullRectUpdate();
            }
          }
        }
      }
    }
    return;
  }

  if (object.SlowFirstChild()) {
    // This ensures cull rect update of the child PaintLayers affected by the
    // paint property change on a non-PaintLayer. Though this may unnecessarily
    // force update of unrelated children, the situation is rare and this is
    // much easier.
    painting_layer.SetForcesChildrenCullRectUpdate();
  }
}

OverriddenCullRectScope::OverriddenCullRectScope(PaintLayer& starting_layer,
                                                 const CullRect& cull_rect)
    : starting_layer_(starting_layer) {
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
  if (updated_)
    starting_layer_.SetNeedsCullRectUpdate();
}

}  // namespace blink
