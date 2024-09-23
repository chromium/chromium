// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

float ExpansionRatio(const LayoutObject& object) {
  const int dpr_coef = features::kCullRectExpansionDPRCoef.Get();
  float device_pixel_ratio =
      object.GetFrame()->LocalFrameRoot().GetDocument()->DevicePixelRatio();
  return 1 + (device_pixel_ratio - 1) * dpr_coef;
}

using FragmentCullRects = OverriddenCullRectScope::FragmentCullRects;
// This is set to non-null when we are updating overridden cull rects for
// special painting. The current cull rects will be saved during the update,
// and will be restored when we exit the OverriddenCullRectScope.
Vector<FragmentCullRects>* g_original_cull_rects = nullptr;

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

  if (g_original_cull_rects) {
    g_original_cull_rects->emplace_back(fragment);
  } else {
    SetLayerNeedsRepaintOnCullRectChange(layer);
  }

  fragment.SetCullRect(cull_rect);
}

// Returns true if the contents cull rect changed.
bool SetFragmentContentsCullRect(PaintLayer& layer,
                                 FragmentData& fragment,
                                 const CullRect& contents_cull_rect) {
  if (contents_cull_rect == fragment.GetContentsCullRect())
    return false;

  if (g_original_cull_rects) {
    if (g_original_cull_rects->empty() ||
        g_original_cull_rects->back().fragment != &fragment) {
      g_original_cull_rects->emplace_back(fragment);
    }
  } else {
    SetLayerNeedsRepaintOnCullRectChange(layer);
    if (auto* scrollable_area = layer.GetScrollableArea())
      scrollable_area->DidUpdateCullRect();
  }

  fragment.SetContentsCullRect(contents_cull_rect);
  return true;
}

bool ShouldUseInfiniteCullRect(
    const PaintLayer& layer,
    ViewTransitionSupplement* view_transition_supplement,
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
      if (transform->Matrix().HasPerspective()) {
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

  if (view_transition_supplement) {
    auto* transition = view_transition_supplement->GetTransition();

    // This means that the contents of the object are drawn elsewhere, so we
    // shouldn't cull it.
    if (transition && transition->IsRepresentedViaPseudoElements(object))
      return true;
  }

  return false;
}

bool HasScrolledEnough(const LayoutObject& object) {
  if (const auto* properties = object.FirstFragment().PaintProperties()) {
    if (const auto* scroll_translation = properties->ScrollTranslation()) {
      const auto* scrollable_area = To<LayoutBox>(object).GetScrollableArea();
      DCHECK(scrollable_area);
      gfx::Vector2dF delta = -scroll_translation->Get2dTranslation() -
                             scrollable_area->LastCullRectUpdateScrollPosition()
                                 .OffsetFromOrigin();
      return object.FirstFragment().GetContentsCullRect().HasScrolledEnough(
          delta, *scroll_translation, ExpansionRatio(object));
    }
  }
  return false;
}

}  // anonymous namespace

CullRectUpdater::CullRectUpdater(PaintLayer& starting_layer,
                                 bool disable_expansion)
    : starting_layer_(starting_layer),
      expansion_ratio_(disable_expansion
                           ? 0.f
                           : ExpansionRatio(starting_layer.GetLayoutObject())) {
  view_transition_supplement_ = ViewTransitionSupplement::FromIfExists(
      starting_layer.GetLayoutObject().GetDocument());
}

void CullRectUpdater::Update() {
  DCHECK(starting_layer_.IsRootLayer());
  TRACE_EVENT0("blink,benchmark", "CullRectUpdate");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Blink.CullRect.UpdateTime");

  UpdateInternal(CullRect::Infinite());

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "PaintLayer tree after cull rect update:";
    ShowLayerTree(&starting_layer_);
  }
#endif
}

void CullRectUpdater::UpdateForTesting(const CullRect& input_cull_rect) {
  DCHECK(starting_layer_.IsRootLayer());
  UpdateInternal(input_cull_rect);
}

void CullRectUpdater::UpdateInternal(const CullRect& input_cull_rect) {
  const auto& object = starting_layer_.GetLayoutObject();
  if (object.GetFrameView()->ShouldThrottleRendering())
    return;
  if (object.IsFragmentLessBox()) {
    return;
  }

  object.GetFrameView()->SetCullRectNeedsUpdateForFrames(
      /*disable_expansion=*/expansion_ratio_ == 0);

  if (!starting_layer_.NeedsCullRectUpdate() &&
      !starting_layer_.DescendantNeedsCullRectUpdate() &&
      // This allows proactive cull rect update for direct children that will
      // be repainted.
      !starting_layer_.SelfOrDescendantNeedsRepaint() &&
      // Don't skip cull rect update with custom input_cull_rect.
      input_cull_rect.IsInfinite()) {
    return;
  }

  root_state_ =
      object.View()->FirstFragment().LocalBorderBoxProperties().Unalias();
  Context context;
  context.current.container = &starting_layer_;
  bool should_use_infinite = ShouldUseInfiniteCullRect(
      starting_layer_, view_transition_supplement_,
      context.current.subtree_should_use_infinite_cull_rect);

  auto& fragment = object.GetMutableForPainting().FirstFragment();
  SetFragmentCullRect(
      starting_layer_, fragment,
      should_use_infinite ? CullRect::Infinite() : input_cull_rect);
  context.current.force_update_children = SetFragmentContentsCullRect(
      starting_layer_, fragment,
      should_use_infinite
          ? CullRect::Infinite()
          : ComputeFragmentContentsCullRect(context, starting_layer_, fragment,
                                            input_cull_rect));

  context.absolute = context.fixed = context.current;
  UpdateForDescendants(context, starting_layer_);

  if (!g_original_cull_rects)
    starting_layer_.ClearNeedsCullRectUpdate();
}

// See UpdateForDescendants for how |force_update_self| is propagated.
void CullRectUpdater::UpdateRecursively(const Context& parent_context,
                                        PaintLayer& layer) {
  if (layer.IsUnderSVGHiddenContainer())
    return;

  const auto& object = layer.GetLayoutObject();
  if (object.IsFragmentLessBox()) {
    return;
  }

  Context context = parent_context;
  if (object.IsAbsolutePositioned())
    context.current = context.absolute;
  if (object.IsFixedPositioned())
    context.current = context.fixed;

  bool should_proactively_update = ShouldProactivelyUpdate(context, layer);
  bool force_update_self = context.current.force_update_children;
  context.current.force_update_children =
      should_proactively_update || layer.ForcesChildrenCullRectUpdate();

  if (force_update_self || should_proactively_update ||
      layer.NeedsCullRectUpdate()) {
    context.current.force_update_children |= UpdateForSelf(context, layer);
  }

  if (!context.current.subtree_is_out_of_cull_rect &&
      object.ShouldClipOverflowAlongBothAxis() && !object.IsFragmented()) {
    const auto* box = layer.GetLayoutBox();
    DCHECK(box);
    PhysicalRect clip_rect =
        box->OverflowClipRect(box->FirstFragment().PaintOffset());
    if (!box->FirstFragment().GetCullRect().Intersects(
            ToEnclosingRect(clip_rect))) {
      context.current.subtree_is_out_of_cull_rect = true;
    }
  }

  bool should_traverse_children =
      context.current.force_update_children ||
      layer.DescendantNeedsCullRectUpdate() ||
      (context.absolute.force_update_children &&
       layer.HasNonContainedAbsolutePositionDescendant()) ||
      (context.fixed.force_update_children &&
       !object.CanContainFixedPositionObjects() &&
       layer.HasFixedPositionDescendant());
  if (should_traverse_children) {
    context.current.container = &layer;
    // We pretend the starting layer can contain all descendants.
    if (&layer == &starting_layer_ ||
        object.CanContainAbsolutePositionObjects()) {
      context.absolute = context.current;
    }
    if (&layer == &starting_layer_ || object.CanContainFixedPositionObjects()) {
      context.fixed = context.current;
    }
    UpdateForDescendants(context, layer);
  }

  if (!g_original_cull_rects)
    layer.ClearNeedsCullRectUpdate();
}

// "Children" in |force_update_children| means children in the containing block
// tree. The flag is set by the containing block whose contents cull rect
// changed.
void CullRectUpdater::UpdateForDescendants(const Context& context,
                                           PaintLayer& layer) {
  const auto& object = layer.GetLayoutObject();

  // DisplayLockContext will force cull rect update of the subtree on unlock.
  if (object.ChildPaintBlockedByDisplayLock())
    return;

  for (auto* child = layer.FirstChild(); child; child = child->NextSibling())
    UpdateRecursively(context, *child);

  if (auto* embedded_content = DynamicTo<LayoutEmbeddedContent>(object)) {
    if (auto* embedded_view = embedded_content->GetEmbeddedContentView()) {
      if (auto* embedded_frame_view =
              DynamicTo<LocalFrameView>(embedded_view)) {
        PaintLayer* subframe_root_layer = nullptr;
        if (auto* sub_layout_view = embedded_frame_view->GetLayoutView())
          subframe_root_layer = sub_layout_view->Layer();
        if (embedded_frame_view->ShouldThrottleRendering()) {
          if (context.current.force_update_children && subframe_root_layer)
            subframe_root_layer->SetNeedsCullRectUpdate();
        } else {
          DCHECK(subframe_root_layer);

          Context subframe_context = {context.current, context.current,
                                      context.current};
          UpdateRecursively(subframe_context, *subframe_root_layer);
        }
      }
    }
  }
}

bool CullRectUpdater::UpdateForSelf(Context& context, PaintLayer& layer) {
  const auto& parent_object = context.current.container->GetLayoutObject();
  // If the containing layer is fragmented, try to match fragments from the
  // container to |layer|, so that any fragment clip for
  // |context.current.container|'s fragment matches |layer|'s.
  //
  // TODO(paint-dev): If nested fragmentation is involved, we're not matching
  // correctly here. In order to fix that, we most likely need to move over to
  // some sort of fragment tree traversal (rather than pure PaintLayer tree
  // traversal).
  bool should_match_fragments = parent_object.IsFragmented();
  bool force_update_children = false;
  bool should_use_infinite_cull_rect =
      !context.current.subtree_is_out_of_cull_rect &&
      ShouldUseInfiniteCullRect(
          layer, view_transition_supplement_,
          context.current.subtree_should_use_infinite_cull_rect);

  for (FragmentData& fragment :
       MutableFragmentDataIterator(layer.GetLayoutObject())) {
    CullRect cull_rect;
    CullRect contents_cull_rect;
    if (context.current.subtree_is_out_of_cull_rect) {
      // PaintLayerPainter may skip the subtree including this layer, so we
      // need to SetPreviousPaintResult() here.
      layer.SetPreviousPaintResult(kMayBeClippedByCullRect);
    } else {
      const FragmentData* parent_fragment = nullptr;
      if (!should_use_infinite_cull_rect) {
        if (should_match_fragments) {
          for (const FragmentData& walker :
               FragmentDataIterator(parent_object)) {
            parent_fragment = &walker;
            if (parent_fragment->FragmentID() == fragment.FragmentID()) {
              break;
            }
          }
        } else {
          parent_fragment = &parent_object.FirstFragment();
        }
      }

      if (should_use_infinite_cull_rect || !parent_fragment) {
        cull_rect = CullRect::Infinite();
        contents_cull_rect = CullRect::Infinite();
      } else {
        cull_rect =
            ComputeFragmentCullRect(context, layer, fragment, *parent_fragment);
        contents_cull_rect = ComputeFragmentContentsCullRect(
            context, layer, fragment, cull_rect);
      }
    }

    SetFragmentCullRect(layer, fragment, cull_rect);
    force_update_children |=
        SetFragmentContentsCullRect(layer, fragment, contents_cull_rect);
  }

  return force_update_children;
}

CullRect CullRectUpdater::ComputeFragmentCullRect(
    Context& context,
    PaintLayer& layer,
    const FragmentData& fragment,
    const FragmentData& parent_fragment) {
  auto local_state = fragment.LocalBorderBoxProperties().Unalias();
  CullRect cull_rect = parent_fragment.GetContentsCullRect();
  auto parent_state = parent_fragment.ContentsProperties().Unalias();
  const LayoutObject& object = layer.GetLayoutObject();
  const auto& parent_object = context.current.container->GetLayoutObject();
  const LocalFrameView* frame_view = object.GetFrameView();
  const LayoutView& layout_view = *object.View();
  const PaginationState* pagination_state = frame_view->GetPaginationState();
  if (parent_object.IsLayoutView() && pagination_state) {
    parent_state = pagination_state->ContentAreaPropertyTreeStateForCurrentPage(
        layout_view);
  }

  if (object.IsFixedPositioned()) {
    if (const auto* properties = fragment.PaintProperties()) {
      if (const auto* translation = properties->PaintOffsetTranslation()) {
        const auto& view_fragment = object.View()->FirstFragment();
        auto root_contents_state =
            view_fragment.LocalBorderBoxProperties().Unalias();
        if (pagination_state) {
          // Document contents are parented under the pagination properties,
          // which in turn are parented under the LayoutView.
          root_contents_state =
              pagination_state->ContentAreaPropertyTreeStateForCurrentPage(
                  layout_view);
        }
        if (translation->Parent() == &root_contents_state.Transform()) {
          // Use the viewport / page area clip and ignore additional clips
          // (e.g. clip-paths) because they are applied on this fixed-position
          // layer by non-containers which may change location relative to this
          // layer on viewport scroll for which we don't want to change
          // fixed-position cull rects for performance.
          if (pagination_state) {
            local_state.SetClip(root_contents_state.Clip());
          } else {
            local_state.SetClip(
                view_fragment.ContentsProperties().Clip().Unalias());
          }
          parent_state = root_contents_state;
          cull_rect = view_fragment.GetCullRect();
        }
      }
    }
  }

  if (parent_state != local_state) {
    std::optional<CullRect> old_cull_rect;
    // Not using |old_cull_rect| will force the cull rect to be updated
    // (skipping |ChangedEnough|) in |ApplyPaintProperties|.
    if (!ShouldProactivelyUpdate(context, layer))
      old_cull_rect = fragment.GetCullRect();
    bool expanded =
        cull_rect.ApplyPaintProperties(root_state_, parent_state, local_state,
                                       old_cull_rect, expansion_ratio_);
    if (expanded && fragment.GetCullRect() != cull_rect)
      context.current.force_proactive_update = true;
  }
  return cull_rect;
}

CullRect CullRectUpdater::ComputeFragmentContentsCullRect(
    Context& context,
    PaintLayer& layer,
    const FragmentData& fragment,
    const CullRect& cull_rect) {
  auto local_state = fragment.LocalBorderBoxProperties().Unalias();
  CullRect contents_cull_rect = cull_rect;
  auto contents_state = fragment.ContentsProperties().Unalias();
  if (contents_state != local_state) {
    std::optional<CullRect> old_contents_cull_rect;
    // Not using |old_cull_rect| will force the cull rect to be updated
    // (skipping |CullRect::ChangedEnough|) in |ApplyPaintProperties|.
    if (!ShouldProactivelyUpdate(context, layer))
      old_contents_cull_rect = fragment.GetContentsCullRect();
    bool expanded = contents_cull_rect.ApplyPaintProperties(
        root_state_, local_state, contents_state, old_contents_cull_rect,
        expansion_ratio_);
    if (expanded && fragment.GetContentsCullRect() != contents_cull_rect)
      context.current.force_proactive_update = true;
  }
  return contents_cull_rect;
}

bool CullRectUpdater::ShouldProactivelyUpdate(const Context& context,
                                              const PaintLayer& layer) const {
  if (context.current.force_proactive_update)
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
    const PaintPropertiesChangeInfo& properties_changed) {
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

  bool should_use_infinite_cull_rect = false;
  if (object.HasLayer()) {
    bool subtree_should_use_infinite_cull_rect = false;
    auto* view_transition_supplement =
        ViewTransitionSupplement::FromIfExists(object.GetDocument());
    should_use_infinite_cull_rect = ShouldUseInfiniteCullRect(
        *To<LayoutBoxModelObject>(object).Layer(), view_transition_supplement,
        subtree_should_use_infinite_cull_rect);
    if (should_use_infinite_cull_rect &&
        object.FirstFragment().GetCullRect().IsInfinite() &&
        object.FirstFragment().GetContentsCullRect().IsInfinite()) {
      return;
    }
  }

  // Cull rects depend on transforms, clip rects, scroll contents sizes and
  // scroll offsets.
  bool needs_cull_rect_update =
      properties_changed.transform_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues ||
      properties_changed.clip_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues ||
      properties_changed.scroll_changed >=
          PaintPropertyChangeType::kChangedOnlySimpleValues ||
      HasScrolledEnough(object);

  if (!needs_cull_rect_update) {
    // For cases that the transform change can be directly updated, we should
    // use infinite cull rect or rect expanded for composied scroll (in case of
    // not scrolled enough) to avoid cull rect change and repaint.
    DCHECK(properties_changed.transform_changed !=
               PaintPropertyChangeType::kChangedOnlyCompositedValues ||
           object.IsSVGChild() || should_use_infinite_cull_rect ||
           !HasScrolledEnough(object));
    return;
  }

  if (object.HasLayer()) {
    To<LayoutBoxModelObject>(object).Layer()->SetNeedsCullRectUpdate();
    // Fixed-position cull rects depend on view clip. See
    // ComputeFragmentCullRect().
    if (const auto* layout_view = DynamicTo<LayoutView>(object)) {
      if (const auto* clip_node =
              object.FirstFragment().PaintProperties()->OverflowClip()) {
        if (clip_node->NodeChanged() != PaintPropertyChangeType::kUnchanged) {
          for (const auto& fragment : layout_view->PhysicalFragments()) {
            if (!fragment.HasOutOfFlowFragmentChild()) {
              continue;
            }
            for (const auto& fragment_child : fragment.Children()) {
              if (!fragment_child->IsFixedPositioned()) {
                continue;
              }
              To<LayoutBox>(fragment_child->GetLayoutObject())
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
    object.EnclosingLayer()->SetForcesChildrenCullRectUpdate();
  }
}

bool CullRectUpdater::IsOverridingCullRects() {
  return !!g_original_cull_rects;
}

FragmentCullRects::FragmentCullRects(FragmentData& fragment)
    : fragment(&fragment),
      cull_rect(fragment.GetCullRect()),
      contents_cull_rect(fragment.GetContentsCullRect()) {}

OverriddenCullRectScope::OverriddenCullRectScope(PaintLayer& starting_layer,
                                                 const CullRect& cull_rect,
                                                 bool disable_expansion) {
  outer_original_cull_rects_ = g_original_cull_rects;

  if (starting_layer.IsRootLayer() &&
      starting_layer.GetLayoutObject().GetFrame()->IsLocalRoot() &&
      !starting_layer.NeedsCullRectUpdate() &&
      !starting_layer.DescendantNeedsCullRectUpdate() &&
      cull_rect ==
          starting_layer.GetLayoutObject().FirstFragment().GetCullRect()) {
    // The current cull rects are good.
    return;
  }

  g_original_cull_rects = &original_cull_rects_;
  CullRectUpdater updater(starting_layer, disable_expansion);
  updater.UpdateInternal(cull_rect);
}

OverriddenCullRectScope::~OverriddenCullRectScope() {
  if (outer_original_cull_rects_ == g_original_cull_rects)
    return;

  DCHECK_EQ(g_original_cull_rects, &original_cull_rects_);
  g_original_cull_rects = outer_original_cull_rects_;
  for (FragmentCullRects& cull_rects : original_cull_rects_) {
    cull_rects.fragment->SetCullRect(cull_rects.cull_rect);
    cull_rects.fragment->SetContentsCullRect(cull_rects.contents_cull_rect);
  }
}

}  // namespace blink
