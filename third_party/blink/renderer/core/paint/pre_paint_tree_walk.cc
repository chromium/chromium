// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "cc/base/features.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static void SetNeedsCompositingLayerPropertyUpdate(const LayoutObject& object) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (!object.HasLayer())
    return;

  auto* compositor = object.View()->Compositor();
  if (!compositor)
    return;

  PaintLayer* paint_layer = To<LayoutBoxModelObject>(object).Layer();

  DisableCompositingQueryAsserts disabler;
  // This ensures that CompositingLayerPropertyUpdater::Update will
  // be called and update LayerState for the LayoutView.
  auto* mapping = paint_layer->GetCompositedLayerMapping();
  if (!mapping)
    mapping = paint_layer->GroupedMapping();
  if (!mapping)
    return;

  // These two calls will cause GraphicsLayerUpdater to run on |paint_layer|
  // from with PLC::UpdateIfNeeded.
  compositor->SetNeedsCompositingUpdate(
      kCompositingUpdateAfterCompositingInputChange);
  mapping->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateLocal);
}

void PrePaintTreeWalk::WalkTree(LocalFrameView& root_frame_view) {
  if (root_frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  DCHECK_EQ(root_frame_view.GetFrame().GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);

  PrePaintTreeWalkContext context;

  // GeometryMapper depends on paint properties.
  bool needs_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(root_frame_view, context);
  if (needs_tree_builder_context_update)
    GeometryMapper::ClearCache();

  if (root_frame_view.GetFrame().IsMainFrame()) {
    auto property_changed = VisualViewportPaintPropertyTreeBuilder::Update(
        root_frame_view.GetPage()->GetVisualViewport(),
        *context.tree_builder_context);

    if (property_changed >
        PaintPropertyChangeType::kChangedOnlyCompositedValues) {
      root_frame_view.SetPaintArtifactCompositorNeedsUpdate();
      if (auto* layout_view = root_frame_view.GetLayoutView())
        SetNeedsCompositingLayerPropertyUpdate(*layout_view);
    }
  }

  Walk(root_frame_view, context);
  paint_invalidator_.ProcessPendingDelayedPaintInvalidations();

#if DCHECK_IS_ON()
  if (needs_tree_builder_context_update) {
    if (!RuntimeEnabledFeatures::CullRectUpdateEnabled() && VLOG_IS_ON(2) &&
        root_frame_view.GetLayoutView()) {
      VLOG(2) << "PrePaintTreeWalk::Walk(root_frame_view=" << &root_frame_view
              << ")\nPaintLayer tree:";
      showLayerTree(root_frame_view.GetLayoutView()->Layer());
    }
    if (VLOG_IS_ON(1))
      showAllPropertyTrees(root_frame_view);
  }
#endif

  // If the frame is invalidated, we need to inform the frame's chrome client
  // so that the client will initiate repaint of the contents.
  if (needs_invalidate_chrome_client_) {
    if (auto* client = root_frame_view.GetChromeClient())
      client->InvalidateContainer();
  }
}

void PrePaintTreeWalk::Walk(LocalFrameView& frame_view,
                            const PrePaintTreeWalkContext& parent_context) {
  bool needs_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(frame_view, parent_context);

  if (frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame, and set dirty bits that will be applied when it
    // becomes unthrottled.
    if (LayoutView* layout_view = frame_view.GetLayoutView()) {
      if (needs_tree_builder_context_update) {
        layout_view->AddSubtreePaintPropertyUpdateReason(
            SubtreePaintPropertyUpdateReason::kPreviouslySkipped);
      }
      if (parent_context.paint_invalidator_context.NeedsSubtreeWalk())
        layout_view->SetSubtreeShouldDoFullPaintInvalidation();
      if (parent_context.effective_allowed_touch_action_changed)
        layout_view->MarkEffectiveAllowedTouchActionChanged();
      if (parent_context.blocking_wheel_event_handler_changed)
        layout_view->MarkBlockingWheelEventHandlerChanged();
    }
    return;
  }

  PrePaintTreeWalkContext context(parent_context,
                                  needs_tree_builder_context_update);

  // Block fragmentation doesn't cross frame boundaries.
  context.current_fragmentainer = {};
  context.absolute_positioned_container = {};
  context.fixed_positioned_container = {};
  context.oof_container_candidate_fragment = nullptr;

  // ancestor_scroll_container_paint_layer does not cross frame boundaries.
  context.ancestor_scroll_container_paint_layer = nullptr;
  if (context.tree_builder_context) {
    PaintPropertyTreeBuilder::SetupContextForFrame(
        frame_view, *context.tree_builder_context);
    context.tree_builder_context->supports_composited_raster_invalidation =
        frame_view.GetFrame().GetSettings()->GetAcceleratedCompositingEnabled();
  }

  if (LayoutView* view = frame_view.GetLayoutView()) {
#if DCHECK_IS_ON()
    if (VLOG_IS_ON(3) && needs_tree_builder_context_update) {
      VLOG(3) << "PrePaintTreeWalk::Walk(frame_view=" << &frame_view
              << ")\nLayout tree:";
      showLayoutTree(view);
      VLOG(3) << "Fragment tree:";
      ShowFragmentTree(*view);
    }
#endif

    is_wheel_event_regions_enabled_ =
        base::FeatureList::IsEnabled(::features::kWheelEventRegions);

    Walk(*view, context, /* pre_paint_info */ nullptr);
#if DCHECK_IS_ON()
    view->AssertSubtreeClearedPaintInvalidationFlags();
#endif
  }

  frame_view.GetLayoutShiftTracker().NotifyPrePaintFinished();
}

namespace {

enum class BlockingEventHandlerType {
  kNone,
  kTouchStartOrMoveBlockingEventHandler,
  kWheelBlockingEventHandler,
};

bool HasBlockingEventHandlerHelper(const LocalFrame& frame,
                                   EventTarget& target,
                                   BlockingEventHandlerType event_type) {
  if (!target.HasEventListeners())
    return false;
  const auto& registry = frame.GetEventHandlerRegistry();
  if (BlockingEventHandlerType::kTouchStartOrMoveBlockingEventHandler ==
      event_type) {
    const auto* blocking = registry.EventHandlerTargets(
        EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
    const auto* blocking_low_latency = registry.EventHandlerTargets(
        EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency);
    return blocking->Contains(&target) ||
           blocking_low_latency->Contains(&target);
  } else if (BlockingEventHandlerType::kWheelBlockingEventHandler ==
             event_type) {
    const auto* blocking =
        registry.EventHandlerTargets(EventHandlerRegistry::kWheelEventBlocking);
    return blocking->Contains(&target);
  }
  NOTREACHED();
  return false;
}

bool HasBlockingEventHandlerHelper(const LayoutObject& object,
                                   BlockingEventHandlerType event_type) {
  if (IsA<LayoutView>(object)) {
    auto* frame = object.GetFrame();
    if (HasBlockingEventHandlerHelper(*frame, *frame->DomWindow(), event_type))
      return true;
  }

  auto* node = object.GetNode();
  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(object);
  if (!node && layout_block_flow &&
      layout_block_flow->IsAnonymousBlockContinuation()) {
    // An anonymous continuation does not have handlers so we need to check the
    // DOM ancestor for handlers using |NodeForHitTest|.
    node = object.NodeForHitTest();
  }
  if (!node)
    return false;
  return HasBlockingEventHandlerHelper(*object.GetFrame(), *node, event_type);
}

bool HasBlockingTouchEventHandler(const LayoutObject& object) {
  return HasBlockingEventHandlerHelper(
      object, BlockingEventHandlerType::kTouchStartOrMoveBlockingEventHandler);
}

bool HasBlockingWheelEventHandler(const LayoutObject& object) {
  return HasBlockingEventHandlerHelper(
      object, BlockingEventHandlerType::kWheelBlockingEventHandler);
}
}  // namespace

void PrePaintTreeWalk::UpdateEffectiveAllowedTouchAction(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (object.EffectiveAllowedTouchActionChanged())
    context.effective_allowed_touch_action_changed = true;

  if (context.effective_allowed_touch_action_changed) {
    object.GetMutableForPainting().UpdateInsideBlockingTouchEventHandler(
        context.inside_blocking_touch_event_handler ||
        HasBlockingTouchEventHandler(object));
  }

  if (object.InsideBlockingTouchEventHandler())
    context.inside_blocking_touch_event_handler = true;
}

void PrePaintTreeWalk::UpdateBlockingWheelEventHandler(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (object.BlockingWheelEventHandlerChanged())
    context.blocking_wheel_event_handler_changed = true;

  if (context.blocking_wheel_event_handler_changed) {
    object.GetMutableForPainting().UpdateInsideBlockingWheelEventHandler(
        context.inside_blocking_wheel_event_handler ||
        HasBlockingWheelEventHandler(object));
  }

  if (object.InsideBlockingWheelEventHandler())
    context.inside_blocking_wheel_event_handler = true;
}

void PrePaintTreeWalk::InvalidatePaintForHitTesting(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (context.paint_invalidator_context.subtree_flags &
      PaintInvalidatorContext::kSubtreeNoInvalidation)
    return;

  if (!context.effective_allowed_touch_action_changed &&
      !context.blocking_wheel_event_handler_changed)
    return;

  context.paint_invalidator_context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidator(object).InvalidateDisplayItemClient(
      object, PaintInvalidationReason::kHitTest);
  SetNeedsCompositingLayerPropertyUpdate(object);
}

void PrePaintTreeWalk::UpdateAuxiliaryObjectProperties(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (!object.HasLayer())
    return;

  PaintLayer* paint_layer = To<LayoutBoxModelObject>(object).Layer();
  paint_layer->UpdateAncestorScrollContainerLayer(
      context.ancestor_scroll_container_paint_layer);

  if (object.IsScrollContainer())
    context.ancestor_scroll_container_paint_layer = paint_layer;
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LocalFrameView& frame_view,
    const PrePaintTreeWalkContext& context) {
  if (frame_view.GetFrame().IsMainFrame() &&
      frame_view.GetPage()->GetVisualViewport().NeedsPaintPropertyUpdate()) {
    return true;
  }

  return frame_view.GetLayoutView() &&
         NeedsTreeBuilderContextUpdate(*frame_view.GetLayoutView(), context);
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& parent_context) {
  return ContextRequiresChildTreeBuilderContext(parent_context) ||
         ObjectRequiresTreeBuilderContext(object);
}

bool PrePaintTreeWalk::ObjectRequiresPrePaint(const LayoutObject& object) {
  return object.ShouldCheckForPaintInvalidation() ||
         object.EffectiveAllowedTouchActionChanged() ||
         object.DescendantEffectiveAllowedTouchActionChanged() ||
         object.BlockingWheelEventHandlerChanged() ||
         object.DescendantBlockingWheelEventHandlerChanged();
  ;
}

bool PrePaintTreeWalk::ContextRequiresChildPrePaint(
    const PrePaintTreeWalkContext& context) {
  return context.paint_invalidator_context.NeedsSubtreeWalk() ||
         context.effective_allowed_touch_action_changed ||
         context.blocking_wheel_event_handler_changed || context.clip_changed;
}

bool PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(
    const LayoutObject& object) {
  return object.NeedsPaintPropertyUpdate() ||
         object.ShouldCheckGeometryForPaintInvalidation() ||
         (!object.ChildPrePaintBlockedByDisplayLock() &&
          (object.DescendantNeedsPaintPropertyUpdate() ||
           object.DescendantShouldCheckGeometryForPaintInvalidation()));
}

bool PrePaintTreeWalk::ContextRequiresChildTreeBuilderContext(
    const PrePaintTreeWalkContext& context) {
  if (!context.NeedsTreeBuilderContext()) {
    DCHECK(!context.tree_builder_context->force_subtree_update_reasons);
    DCHECK(!context.paint_invalidator_context.NeedsSubtreeWalk());
    return false;
  }
  return context.tree_builder_context->force_subtree_update_reasons ||
         // PaintInvalidator forced subtree walk implies geometry update.
         context.paint_invalidator_context.NeedsSubtreeWalk();
}

#if DCHECK_IS_ON()
void PrePaintTreeWalk::CheckTreeBuilderContextState(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& parent_context) {
  if (parent_context.tree_builder_context ||
      (!ObjectRequiresTreeBuilderContext(object) &&
       !ContextRequiresChildTreeBuilderContext(parent_context))) {
    return;
  }

  DCHECK(!object.NeedsPaintPropertyUpdate());
  DCHECK(!object.DescendantNeedsPaintPropertyUpdate());
  DCHECK(!object.DescendantShouldCheckGeometryForPaintInvalidation());
  DCHECK(!object.ShouldCheckGeometryForPaintInvalidation());
  NOTREACHED() << "Unknown reason.";
}
#endif

static LayoutBoxModelObject* ContainerForPaintInvalidation(
    const PaintLayer* painting_layer) {
  if (!painting_layer)
    return nullptr;
  if (auto* containing_paint_layer =
          painting_layer
              ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries())
    return &containing_paint_layer->GetLayoutObject();
  return nullptr;
}

void PrePaintTreeWalk::UpdatePaintInvalidationContainer(
    const LayoutObject& object,
    const PaintLayer* painting_layer,
    PrePaintTreeWalkContext& context,
    bool is_ng_painting) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  DisableCompositingQueryAsserts disabler;

  if (object.IsPaintInvalidationContainer()) {
    context.paint_invalidation_container = To<LayoutBoxModelObject>(&object);
    if (object.IsStackingContext() || object.IsSVGRoot()) {
      context.paint_invalidation_container_for_stacked_contents =
          To<LayoutBoxModelObject>(&object);
    }
  } else if (IsA<LayoutView>(object)) {
    // paint_invalidation_container_for_stacked_contents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames.
    // Contents stacked in the root stacking context in this frame should use
    // this frame's PaintInvalidationContainer.
    context.paint_invalidation_container_for_stacked_contents =
        ContainerForPaintInvalidation(painting_layer);
  } else if (!is_ng_painting &&
             (object.IsColumnSpanAll() ||
              object.IsFloatingWithNonContainingBlockParent())) {
    // In these cases, the object may belong to an ancestor of the current
    // paint invalidation container, in paint order.
    // Post LayoutNG the |LayoutObject::IsFloatingWithNonContainingBlockParent|
    // check can be removed as floats will be painted by the correct layer.
    context.paint_invalidation_container =
        ContainerForPaintInvalidation(painting_layer);
  } else if (object.IsStacked() &&
             // This is to exclude some objects (e.g. LayoutText) inheriting
             // stacked style from parent but aren't actually stacked.
             object.HasLayer() &&
             !To<LayoutBoxModelObject>(object)
                  .Layer()
                  ->IsReplacedNormalFlowStacking() &&
             context.paint_invalidation_container !=
                 context.paint_invalidation_container_for_stacked_contents) {
    // The current object is stacked, so we should use
    // m_paintInvalidationContainerForStackedContents as its paint invalidation
    // container on which the current object is painted.
    context.paint_invalidation_container =
        context.paint_invalidation_container_for_stacked_contents;
  }
}

NGPrePaintInfo PrePaintTreeWalk::CreatePrePaintInfo(
    const NGLink& child,
    const PrePaintTreeWalkContext& context) {
  const auto& fragment = *To<NGPhysicalBoxFragment>(child.fragment);
  return NGPrePaintInfo(fragment, child.offset,
                        context.current_fragmentainer.fragmentainer_idx,
                        fragment.IsFirstForNode(), !fragment.BreakToken(),
                        context.is_inside_orphaned_object,
                        /* is_inside_fragment_child */ false);
}

FragmentData* PrePaintTreeWalk::GetOrCreateFragmentData(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& context,
    const NGPrePaintInfo& pre_paint_info) {
  // If |allow_update| is set, we're allowed to add, remove and modify
  // FragmentData objects. Otherwise they will be left alone.
  bool allow_update = context.NeedsTreeBuilderContext();

  FragmentData* fragment_data = &object.GetMutableForPainting().FirstFragment();

  // The need for paint properties is the same across all fragments, so if the
  // first FragmentData needs it, so do all the others.
  bool needs_paint_properties = fragment_data->PaintProperties();

  wtf_size_t fragment_id = pre_paint_info.fragmentainer_idx;
  // TODO(mstensho): For now we need to treat unfragmented as ID 0. It doesn't
  // really matter for LayoutNG, but legacy
  // PaintPropertyTreeBuilder::ContextForFragment() may take a walk up the tree
  // and end up querying this (LayoutNG) object, and
  // FragmentData::LogicalTopInFlowThread() will DCHECK that the value is 0
  // unless it has been explicitly set by legacy code (which won't happen, since
  // it's an NG object).
  if (fragment_id == WTF::kNotFound)
    fragment_id = 0;

  if (pre_paint_info.is_first_for_node) {
    if (allow_update)
      fragment_data->ClearNextFragment();
    else
      DCHECK_EQ(fragment_data->FragmentID(), fragment_id);
  } else {
    FragmentData* last_fragment = nullptr;
    do {
      if (fragment_data->FragmentID() >= fragment_id)
        break;
      last_fragment = fragment_data;
      fragment_data = fragment_data->NextFragment();
    } while (fragment_data);
    if (fragment_data) {
      if (pre_paint_info.is_last_for_node) {
        // We have reached the end. There may be more data entries that were
        // needed in the previous layout, but not any more. Clear them.
        if (allow_update)
          fragment_data->ClearNextFragment();
        else
          DCHECK(!fragment_data->NextFragment());
      } else if (fragment_data->FragmentID() != fragment_id) {
        // There are entries for fragmentainers after this one, but none for
        // this one. Remove the fragment tail.
        DCHECK(allow_update);
        DCHECK_GT(fragment_data->FragmentID(), fragment_id);
        fragment_data->ClearNextFragment();
      }
    } else {
      DCHECK(allow_update);
      fragment_data = &last_fragment->EnsureNextFragment();
    }
  }

  if (allow_update) {
    fragment_data->SetFragmentID(fragment_id);

    if (needs_paint_properties)
      fragment_data->EnsurePaintProperties();
  } else {
    DCHECK_EQ(fragment_data->FragmentID(), fragment_id);
    DCHECK(!needs_paint_properties || fragment_data->PaintProperties());
  }

  return fragment_data;
}

void PrePaintTreeWalk::WalkInternal(const LayoutObject& object,
                                    PrePaintTreeWalkContext& context,
                                    NGPrePaintInfo* pre_paint_info) {
  PaintInvalidatorContext& paint_invalidator_context =
      context.paint_invalidator_context;

  if (pre_paint_info) {
    DCHECK(!pre_paint_info->fragment_data);
    // Find, update or create a FragmentData object to match the current block
    // fragment.
    //
    // TODO(mstensho): If this is collapsed text or a culled inline, we might
    // not have any work to do (we could just return early here), as there'll be
    // no need for paint property updates or invalidation. However, this is a
    // bit tricky to determine, because of things like LinkHighlight, which
    // might set paint properties on a culled inline.
    pre_paint_info->fragment_data =
        GetOrCreateFragmentData(object, context, *pre_paint_info);
    DCHECK(pre_paint_info->fragment_data);
  }

  // This must happen before updatePropertiesForSelf, because the latter reads
  // some of the state computed here.
  UpdateAuxiliaryObjectProperties(object, context);

  absl::optional<PaintPropertyTreeBuilder> property_tree_builder;
  PaintPropertyChangeType property_changed =
      PaintPropertyChangeType::kUnchanged;
  if (context.tree_builder_context) {
    property_tree_builder.emplace(object, pre_paint_info,
                                  *context.tree_builder_context);

    property_changed =
        std::max(property_changed, property_tree_builder->UpdateForSelf());

    if ((property_changed > PaintPropertyChangeType::kUnchanged) &&
        !context.tree_builder_context
             ->supports_composited_raster_invalidation) {
      paint_invalidator_context.subtree_flags |=
          PaintInvalidatorContext::kSubtreeFullInvalidation;
    }
  }

  // This must happen before paint invalidation because background painting
  // depends on the effective allowed touch action and blocking wheel event
  // handlers.
  UpdateEffectiveAllowedTouchAction(object, context);
  if (is_wheel_event_regions_enabled_)
    UpdateBlockingWheelEventHandler(object, context);

  if (paint_invalidator_.InvalidatePaint(
          object, pre_paint_info,
          base::OptionalOrNullptr(context.tree_builder_context),
          paint_invalidator_context))
    needs_invalidate_chrome_client_ = true;

  InvalidatePaintForHitTesting(object, context);

  UpdatePaintInvalidationContainer(object,
                                   paint_invalidator_context.painting_layer,
                                   context, !!pre_paint_info);

  if (context.tree_builder_context) {
    property_changed =
        std::max(property_changed, property_tree_builder->UpdateForChildren());

    if (!RuntimeEnabledFeatures::CullRectUpdateEnabled() &&
        context.tree_builder_context->clip_changed) {
      // Save clip_changed flag in |context| so that all descendants will see it
      // even if we don't create tree_builder_context.
      context.clip_changed = true;
    }

    if (property_changed != PaintPropertyChangeType::kUnchanged) {
      if (property_changed >
          PaintPropertyChangeType::kChangedOnlyCompositedValues) {
        object.GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
      }

      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        if ((property_changed >
             PaintPropertyChangeType::kChangedOnlyCompositedValues) &&
            context.paint_invalidation_container) {
          // Mark the previous paint invalidation container as needing
          // raster invalidation. This handles cases where raster invalidation
          // needs to happen but no compositing layers were added or removed.
          DisableCompositingQueryAsserts disabler;

          const auto* paint_invalidation_container =
              context.paint_invalidation_container->Layer();
          if (!paint_invalidation_container->SelfNeedsRepaint()) {
            auto* mapping =
                paint_invalidation_container->GetCompositedLayerMapping();
            if (!mapping)
              mapping = paint_invalidation_container->GroupedMapping();
            if (mapping)
              mapping->SetNeedsCheckRasterInvalidation();
          }

          SetNeedsCompositingLayerPropertyUpdate(object);
        }
      } else if (!context.tree_builder_context
                      ->supports_composited_raster_invalidation) {
        paint_invalidator_context.subtree_flags |=
            PaintInvalidatorContext::kSubtreeFullInvalidation;
      }
    }
  }

  if (RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
    if (property_changed != PaintPropertyChangeType::kUnchanged ||
        // CullRectUpdater proactively update cull rect if the layer or
        // descendant will repaint, but in pre-CAP the repaint flag stops
        // propagation at compositing boundaries, while cull rect update
        // ancestor flag should not stop at compositing boundaries.
        (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
         context.paint_invalidator_context.painting_layer
             ->SelfOrDescendantNeedsRepaint())) {
      if (object.HasLayer()) {
        To<LayoutBoxModelObject>(object).Layer()->SetNeedsCullRectUpdate();
      } else if (object.SlowFirstChild()) {
        // This ensures cull rect update of the child PaintLayers affected by
        // the paint property change on a non-PaintLayer. Though this may
        // unnecessarily force update of unrelated children, the situation is
        // rare and this is much easier.
        context.paint_invalidator_context.painting_layer
            ->SetForcesChildrenCullRectUpdate();
      }
    }
  } else if (context.clip_changed && object.HasLayer()) {
    // When this or ancestor clip changed, the layer needs repaint because it
    // may paint more or less results according to the changed clip.
    To<LayoutBoxModelObject>(object).Layer()->SetNeedsRepaint();
  }
}

bool PrePaintTreeWalk::CollectMissableChildren(
    PrePaintTreeWalkContext& context,
    const NGPhysicalBoxFragment& parent) {
  bool has_missable_children = false;
  for (const NGLink& child : parent.Children()) {
    if ((child->IsOutOfFlowPositioned() &&
         (context.current_fragmentainer.fragment ||
          child->IsFixedPositioned())) ||
        (child->IsFloating() && parent.IsInlineFormattingContext() &&
         context.current_fragmentainer.fragment)) {
      // We'll add resumed floats (or floats that couldn't fit a fragment in the
      // fragmentainer where it was discovered) that have escaped their inline
      // formatting context.
      //
      // We'll also add all out-of-flow positioned fragments inside a
      // fragmentation context. If a fragment is fixed-positioned, we even need
      // to add those that aren't inside a fragmentation context, because they
      // may have an ancestor LayoutObject inside one, and one of those
      // ancestors may be out-of-flow positioned, which may be missed, in which
      // case we'll miss this fixed-positioned one as well (since we don't enter
      // descendant OOFs when walking missed children) (example: fixedpos inside
      // missed abspos in relpos in multicol).
      pending_missables_.insert(child.fragment);
      has_missable_children = true;
    }
  }
  return has_missable_children;
}

void PrePaintTreeWalk::WalkMissedChildren(const NGPhysicalBoxFragment& fragment,
                                          PrePaintTreeWalkContext& context) {
  if (pending_missables_.IsEmpty())
    return;

  for (const NGLink& child : fragment.Children()) {
    if (!child->IsOutOfFlowPositioned() && !child->IsFloating())
      continue;
    if (!pending_missables_.Contains(child.fragment))
      continue;
    const LayoutObject& descendant_object = *child->GetLayoutObject();
    PrePaintTreeWalkContext descendant_context(
        context, NeedsTreeBuilderContextUpdate(descendant_object, context));
    if (child->IsOutOfFlowPositioned() &&
        descendant_context.tree_builder_context) {
      PaintPropertyTreeBuilderFragmentContext& fragment_context =
          descendant_context.tree_builder_context->fragments[0];
      // Reset the relevant OOF context to this fragmentainer, since this is its
      // containing block, as far as the NG fragment structure is concerned.
      // Note that when walking a missed child OOF fragment, we'll also
      // forcefully miss any OOF descendant nodes, which is why we only set the
      // context for the OOF type we're dealing with here.
      if (child->IsFixedPositioned())
        fragment_context.fixed_position = fragment_context.current;
      else
        fragment_context.absolute_position = fragment_context.current;
    }
    descendant_context.is_inside_orphaned_object = true;

    NGPrePaintInfo pre_paint_info =
        CreatePrePaintInfo(child, descendant_context);
    Walk(descendant_object, descendant_context, &pre_paint_info);
  }
}

LocalFrameView* FindWebViewPluginContentFrameView(
    const LayoutEmbeddedContent& embedded_content) {
  for (Frame* frame = embedded_content.GetFrame()->Tree().FirstChild(); frame;
       frame = frame->Tree().NextSibling()) {
    if (frame->IsLocalFrame() &&
        To<LocalFrame>(frame)->OwnerLayoutObject() == &embedded_content)
      return To<LocalFrame>(frame)->View();
  }
  return nullptr;
}

void PrePaintTreeWalk::WalkFragmentationContextRootChildren(
    const LayoutObject& object,
    const NGPhysicalBoxFragment& fragment,
    PrePaintTreeWalkContext& context) {
  // The actual children are inside the flow thread child of |object|.
  const auto* flow_thread =
      To<LayoutBlockFlow>(&object)->MultiColumnFlowThread();
  const LayoutObject& actual_parent = flow_thread ? *flow_thread : object;

  FragmentData* fragmentainer_fragment_data = nullptr;
#if DCHECK_IS_ON()
  const LayoutObject* fragmentainer_owner_box = nullptr;
#endif

  DCHECK(fragment.IsFragmentationContextRoot());

  const auto outer_fragmentainer = context.current_fragmentainer;
  absl::optional<wtf_size_t> inner_fragmentainer_idx;

  for (NGLink child : fragment.Children()) {
    const auto* box_fragment = To<NGPhysicalBoxFragment>(child.fragment);
    if (UNLIKELY(box_fragment->IsLayoutObjectDestroyedOrMoved()))
      continue;

    if (box_fragment->GetLayoutObject()) {
      // OOFs contained by a multicol container will be visited during object
      // tree traversal.
      if (box_fragment->IsOutOfFlowPositioned())
        continue;

      // We'll walk all other non-fragmentainer children directly now, entering
      // them from the fragment tree, rather than from the LayoutObject tree.
      // One consequence of this is that paint effects on any ancestors between
      // a column spanner and its multicol container will not be applied on the
      // spanner. This is fixable, but it would require non-trivial amounts of
      // special-code for such a special case. If anyone complains, we can
      // revisit this decision.
      if (box_fragment->IsColumnSpanAll())
        context.current_fragmentainer = outer_fragmentainer;

      NGPrePaintInfo pre_paint_info = CreatePrePaintInfo(child, context);
      Walk(*box_fragment->GetLayoutObject(), context, &pre_paint_info);
      continue;
    }

    // Check |box_fragment| and the |LayoutBox| that produced it are in sync.
    // |OwnerLayoutBox()| has a few DCHECKs for this purpose.
    DCHECK(box_fragment->OwnerLayoutBox());

    // A fragmentainer doesn't paint anything itself. Just include its offset
    // and descend into children.
    DCHECK(box_fragment->IsFragmentainerBox());

    // Always keep track of the current innermost fragmentainer we're handling,
    // as they may serve as containing blocks for OOF descendants.
    context.current_fragmentainer.fragment = box_fragment;

    // Set up |inner_fragmentainer_idx| lazily, as it's O(n) (n == number of
    // multicol container fragments).
    if (!inner_fragmentainer_idx)
      inner_fragmentainer_idx = PreviousInnerFragmentainerIndex(fragment);
    context.current_fragmentainer.fragmentainer_idx = *inner_fragmentainer_idx;

    if (UNLIKELY(!context.tree_builder_context)) {
      WalkChildren(actual_parent, box_fragment, context);
      continue;
    }

    PaintPropertyTreeBuilderContext& tree_builder_context =
        *context.tree_builder_context;
    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        tree_builder_context.fragments[0];
    PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext*
        containing_block_context = &fragment_context.current;
    containing_block_context->paint_offset += child.offset;

    const PhysicalOffset paint_offset = containing_block_context->paint_offset;
    // Keep track of the paint offset at the fragmentainer, and also reset the
    // offset adjustment tracker. This is needed when entering OOF
    // descendants. OOFs have the nearest fragmentainer as their containing
    // block, so when entering them during LayoutObject tree traversal, we have
    // to compensate for this.
    fragment_context.fragmentainer_paint_offset = paint_offset;
    fragment_context.adjustment_for_oof_in_fragmentainer = PhysicalOffset();

    // Create corresponding |FragmentData|. Hit-testing needs
    // |FragmentData.PaintOffset|.
    if (fragmentainer_fragment_data) {
      DCHECK(!box_fragment->IsFirstForNode());
#if DCHECK_IS_ON()
      DCHECK_EQ(fragmentainer_owner_box, box_fragment->OwnerLayoutBox());
#endif
      fragmentainer_fragment_data =
          &fragmentainer_fragment_data->EnsureNextFragment();
    } else {
      const LayoutBox* owner_box = box_fragment->OwnerLayoutBox();
#if DCHECK_IS_ON()
      DCHECK(!fragmentainer_owner_box);
      fragmentainer_owner_box = owner_box;
#endif
      fragmentainer_fragment_data =
          &owner_box->GetMutableForPainting().FirstFragment();
      if (box_fragment->IsFirstForNode()) {
        fragmentainer_fragment_data->ClearNextFragment();
      } else {
        // |box_fragment| is nested in another fragmentainer, and that it is
        // the first one in this loop, but not the first one for the
        // |LayoutObject|. Append a new |FragmentData| to the last one.
        fragmentainer_fragment_data =
            &fragmentainer_fragment_data->LastFragment().EnsureNextFragment();
      }
    }
    fragmentainer_fragment_data->SetPaintOffset(paint_offset);

    WalkChildren(actual_parent, box_fragment, context);

    containing_block_context->paint_offset -= child.offset;
    (*inner_fragmentainer_idx)++;
  }

  if (!flow_thread)
    return;
  // Multicol containers only contain special legacy children invisible to
  // LayoutNG, so we need to clean them manually.
  if (fragment.BreakToken())
    return;  // Wait until we've reached the end.
  for (const LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    DCHECK(child->IsLayoutFlowThread() || child->IsLayoutMultiColumnSet() ||
           child->IsLayoutMultiColumnSpannerPlaceholder());
    child->GetMutableForPainting().ClearPaintFlags();
  }
}

void PrePaintTreeWalk::WalkLayoutObjectChildren(
    const LayoutObject& parent_object,
    const NGPhysicalBoxFragment* parent_fragment,
    PrePaintTreeWalkContext& context) {
  for (const LayoutObject* child = parent_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!parent_fragment) {
      // If we haven't found a fragment tree to accompany us in our walk,
      // perform a pure LayoutObject tree walk. This is needed for legacy block
      // fragmentation, and it also works fine if there's no block fragmentation
      // involved at all (in such cases we can either to do this, or perform the
      // NGPhysicalBoxFragment-accompanied walk that we do further down).

      if (child->IsLayoutMultiColumnSpannerPlaceholder()) {
        child->GetMutableForPainting().ClearPaintFlags();
        continue;
      }

      Walk(*child, context, /* pre_paint_info */ nullptr);
      continue;
    }

    // If we're in the middle of walking a missed OOF, don't enter nested OOFs
    // (but miss those as well, and handle them via fragment traversal).
    if (context.is_inside_orphaned_object && child->IsOutOfFlowPositioned())
      continue;

    // Perform an NGPhysicalBoxFragment-accompanied walk of the child
    // LayoutObject tree.
    //
    // We'll map each child LayoutObject to a corresponding
    // NGPhysicalBoxFragment. For text and non-atomic inlines this will be the
    // fragment of their containing block, while for all other objects, it will
    // be a fragment generated by the object itself. Even when we have LayoutNG
    // fragments, we'll try to do the pre-paint walk it in LayoutObject tree
    // order. This will ensure that paint properties are applied correctly (the
    // LayoutNG fragment tree follows the containing block structure closely,
    // but for paint effects, it's actually the LayoutObject / DOM tree
    // structure that matters, e.g. when there's a relpos with a child with
    // opacity, which has an absolutely positioned child, the absolutely
    // positioned child should be affected by opacity, even if the object that
    // establishes the opacity layer isn't in the containing block
    // chain). Furthermore, culled inlines have no fragments, but they still
    // need to be visited, since the invalidation code marks them for pre-paint.
    const NGPhysicalBoxFragment* box_fragment = nullptr;
    wtf_size_t fragmentainer_idx =
        context.current_fragmentainer.fragmentainer_idx;
    PhysicalOffset paint_offset;
    const auto* child_box = DynamicTo<LayoutBox>(child);
    bool is_first_for_node = true;
    bool is_last_for_node = true;
    bool is_inside_fragment_child = false;
    if (parent_fragment->Items() && child->FirstInlineFragmentItemIndex()) {
      for (const NGFragmentItem& item : parent_fragment->Items()->Items()) {
        if (item.GetLayoutObject() != child)
          continue;

        is_last_for_node = item.IsLastForNode();
        if (box_fragment) {
          if (is_last_for_node)
            break;
          continue;
        }

        paint_offset = item.OffsetInContainerFragment();
        is_first_for_node = item.IsFirstForNode();

        if (item.BoxFragment() && !item.BoxFragment()->IsInlineBox()) {
          box_fragment = item.BoxFragment();
          is_last_for_node = !box_fragment->BreakToken();
          break;
        } else {
          // Inlines will pass their containing block fragment (and its incoming
          // break token).
          box_fragment = parent_fragment;
          is_inside_fragment_child = true;
        }

        if (is_last_for_node)
          break;

        // Keep looking for the end. We need to know whether this is the last
        // time we're going to visit this object.
      }
      if (!box_fragment)
        continue;
    } else if (child->IsInline() && !child_box) {
      // Deal with inline-level objects not searched for above.
      //
      // Needed for fragment-less objects that have children. This is the case
      // for culled inlines. We're going to have to enter them for every
      // fragment in the parent.
      //
      // The child is inline-level even if the parent fragment doesn't establish
      // an inline formatting context. This may happen if there's only collapsed
      // text, or if we had to insert a break in a block before we got to any
      // inline content. Make sure that we visit such objects once.

      // Inlines will pass their containing block fragment (and its incoming
      // break token).
      box_fragment = parent_fragment;
      is_inside_fragment_child = true;

      is_first_for_node = parent_fragment->IsFirstForNode();
      is_last_for_node = !parent_fragment->BreakToken();
    } else if (child_box && child_box->PhysicalFragmentCount()) {
      // Figure out which fragment the child may be found inside. The fragment
      // tree follows the structure of containing blocks closely, while here
      // we're walking the LayoutObject tree (which follows the structure of the
      // flat DOM tree, more or less). This means that for out-of-flow
      // positioned objects, the fragment of the parent LayoutObject might not
      // be the right place to search.
      const NGPhysicalBoxFragment* search_fragment = parent_fragment;
      if (child_box->IsOutOfFlowPositioned()) {
        if (child_box->IsFixedPositioned()) {
          search_fragment = context.fixed_positioned_container.fragment;
          fragmentainer_idx =
              context.fixed_positioned_container.fragmentainer_idx;
        } else {
          search_fragment = context.absolute_positioned_container.fragment;
          fragmentainer_idx =
              context.absolute_positioned_container.fragmentainer_idx;
        }
        if (!search_fragment) {
          // Only walk unfragmented legacy-contained OOFs once.
          if (context.is_inside_orphaned_object ||
              (context.current_fragmentainer.fragment &&
               !context.current_fragmentainer.fragment->IsFirstForNode()))
            continue;
        }
      }

      if (search_fragment) {
        // See if we can find a fragment for the child.
        for (NGLink link : search_fragment->Children()) {
          if (link->GetLayoutObject() != child)
            continue;
          // We found it!
          box_fragment = To<NGPhysicalBoxFragment>(link.get());
          paint_offset = link.offset;
          is_first_for_node = box_fragment->IsFirstForNode();
          is_last_for_node = !box_fragment->BreakToken();
          break;
        }
        // If we didn't find a fragment for the child, it means that the child
        // doesn't occur inside the fragmentainer that we're currently handling.
        if (!box_fragment)
          continue;
      }
    }

    if (box_fragment) {
      NGPrePaintInfo pre_paint_info(
          *box_fragment, paint_offset, fragmentainer_idx, is_first_for_node,
          is_last_for_node, context.is_inside_orphaned_object,
          is_inside_fragment_child);
      Walk(*child, context, &pre_paint_info);
    } else {
      Walk(*child, context, /* pre_paint_info */ nullptr);
    }
  }
}

void PrePaintTreeWalk::WalkChildren(const LayoutObject& object,
                                    const NGPhysicalBoxFragment* fragment,
                                    PrePaintTreeWalkContext& context,
                                    bool is_inside_fragment_child) {
  const LayoutBox* box = DynamicTo<LayoutBox>(&object);
  if (box) {
    if (fragment) {
      if (!box->IsLayoutFlowThread() && (!box->CanTraversePhysicalFragments() ||
                                         !box->PhysicalFragmentCount())) {
        // Leave LayoutNGBoxFragment-accompanied child LayoutObject traversal,
        // since this object doesn't support that (or has no fragments (happens
        // for table columns)). We need to switch back to legacy LayoutObject
        // traversal for its children. We're then also assuming that we're
        // either not block-fragmenting, or that this is monolithic content. We
        // may re-enter LayoutNGBoxFragment-accompanied traversal if we get to a
        // descendant that supports that.
        DCHECK(
            !box->FlowThreadContainingBlock() ||
            (box->GetNGPaginationBreakability() == LayoutBox::kForbidBreaks));

        fragment = nullptr;
        context.oof_container_candidate_fragment = nullptr;
      }
    } else {
      // There may be fragment-less objects, such as table columns or table
      // column groups.
      if (box->CanTraversePhysicalFragments() && box->PhysicalFragmentCount()) {
        // Enter LayoutNGBoxFragment-accompanied child LayoutObject traversal.
        // We'll stay in this mode for all descendants that support fragment
        // traversal. We'll re-enter legacy traversal for descendants that don't
        // support it. This only works correctly if we're not block-fragmented,
        // though, so DCHECK for that.
        //
        // TODO(mstensho): Before shipping LayoutNGFragmentTraversal: Only enter
        // this mode at block fragmentation roots (multicol containers), as
        // LayoutNGBoxFragment-accompanied child LayoutObject traversal is more
        // expensive than pure LayoutObject traversal: we need to search for
        // each object among child fragments (NGLink) to find the offset, also
        // when not fragmented at all. For now, though enter this mode as often
        // as we can, for increased test coverage (when running with
        // LayoutNGFragmentTraversal enabled).
        DCHECK_EQ(box->PhysicalFragmentCount(), 1u);
        fragment = To<NGPhysicalBoxFragment>(box->GetPhysicalFragment(0));
        DCHECK(!fragment->BreakToken());
      }
    }

    // Inline-contained OOFs are placed in the containing block of the
    // containing inline in NG, not an anonymous block that's part of a
    // continuation, if any. We need to know where these might be stored, so
    // that we eventually search the right ancestor fragment for them.
    if (fragment && !box->IsAnonymousBlock())
      context.oof_container_candidate_fragment = fragment;
  }

  // Keep track of fragments that act as containers for OOFs, so that we can
  // search their children when looking for an OOF further down in the tree.
  if (object.CanContainAbsolutePositionObjects()) {
    if (context.current_fragmentainer.fragment && box &&
        box->GetNGPaginationBreakability() == LayoutBox::kForbidBreaks) {
      // If we're in a fragmentation context, the parent fragment of OOFs is the
      // fragmentainer, unless the object is monolithic, in which case nothing
      // inside the object participates in the current block fragmentation
      // context. This means that this object (and not the nearest
      // fragmentainer) acts as a containing block for OOF descendants,
      context.current_fragmentainer = {};
    }
    // The OOF containing block structure is special under block fragmentation:
    // A fragmentable OOF is always a direct child of a fragmentainer.
    const auto* container_fragment = context.current_fragmentainer.fragment;
    if (!container_fragment)
      container_fragment = context.oof_container_candidate_fragment;
    context.absolute_positioned_container = {
        container_fragment, context.current_fragmentainer.fragmentainer_idx};
    if (object.CanContainFixedPositionObjects()) {
      context.fixed_positioned_container = {
          container_fragment, context.current_fragmentainer.fragmentainer_idx};
    }
  }

  if (fragment) {
    bool has_missable_children = false;
    // If we are at a block fragment, collect any missable children.
    DCHECK(!is_inside_fragment_child || !object.IsBox());
    if (!is_inside_fragment_child)
      has_missable_children = CollectMissableChildren(context, *fragment);

    // We'll always walk the LayoutObject tree when possible, but if this is a
    // fragmentation context root (such as a multicol container), we need to
    // enter each fragmentainer child and then walk all the LayoutObject
    // children.
    if (fragment->IsFragmentationContextRoot())
      WalkFragmentationContextRootChildren(object, *fragment, context);
    else
      WalkLayoutObjectChildren(object, fragment, context);

    if (has_missable_children)
      WalkMissedChildren(*fragment, context);
  } else {
    WalkLayoutObjectChildren(object, fragment, context);
  }
}

void PrePaintTreeWalk::Walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parent_context,
                            NGPrePaintInfo* pre_paint_info) {
  const NGPhysicalBoxFragment* physical_fragment = nullptr;
  bool is_inside_fragment_child = false;
  if (pre_paint_info) {
    physical_fragment = &pre_paint_info->box_fragment;
    if (physical_fragment && (physical_fragment->IsOutOfFlowPositioned() ||
                              physical_fragment->IsFloating()))
      pending_missables_.erase(physical_fragment);
    is_inside_fragment_child = pre_paint_info->is_inside_fragment_child;
  }

  bool needs_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(object, parent_context);

#if DCHECK_IS_ON()
  CheckTreeBuilderContextState(object, parent_context);
#endif

  // Early out from the tree walk if possible.
  if (!needs_tree_builder_context_update && !ObjectRequiresPrePaint(object) &&
      !ContextRequiresChildPrePaint(parent_context)) {
    return;
  }

  PrePaintTreeWalkContext context(parent_context,
                                  needs_tree_builder_context_update);

  if (object.StyleRef().HasTransform()) {
    // Ignore clip changes from ancestor across transform boundaries.
    context.clip_changed = false;
    if (context.tree_builder_context)
      context.tree_builder_context->clip_changed = false;
  }

  WalkInternal(object, context, pre_paint_info);

  bool child_walk_blocked = object.ChildPrePaintBlockedByDisplayLock();
  // If we need a subtree walk due to context flags, we need to store that
  // information on the display lock, since subsequent walks might not set the
  // same bits on the context.
  if (child_walk_blocked && (ContextRequiresChildTreeBuilderContext(context) ||
                             ContextRequiresChildPrePaint(context))) {
    // Note that |effective_allowed_touch_action_changed| and
    // |blocking_wheel_event_handler_changed| are special in that they requires
    // us to specifically recalculate this value on each subtree element. Other
    // flags simply need a subtree walk. Some consideration needs to be given to
    // |clip_changed| which ensures that we repaint every layer, but for the
    // purposes of PrePaint, this flag is just forcing a subtree walk.
    object.GetDisplayLockContext()->SetNeedsPrePaintSubtreeWalk(
        context.effective_allowed_touch_action_changed,
        context.blocking_wheel_event_handler_changed);
  }

  if (!child_walk_blocked) {
    WalkChildren(object, physical_fragment, context, is_inside_fragment_child);

    if (const auto* layout_embedded_content =
            DynamicTo<LayoutEmbeddedContent>(object)) {
      if (auto* embedded_view =
              layout_embedded_content->GetEmbeddedContentView()) {
        if (context.tree_builder_context) {
          auto& current = context.tree_builder_context->fragments[0].current;
          current.paint_offset = PhysicalOffset(RoundedIntPoint(
              current.paint_offset +
              layout_embedded_content->ReplacedContentRect().offset -
              PhysicalOffset(embedded_view->FrameRect().Location())));
          // Subpixel accumulation doesn't propagate across embedded view.
          current.directly_composited_container_paint_offset_subpixel_delta =
              PhysicalOffset();
        }
        if (embedded_view->IsLocalFrameView()) {
          Walk(*To<LocalFrameView>(embedded_view), context);
        } else if (embedded_view->IsPluginView()) {
          // If it is a webview plugin, walk into the content frame view.
          if (auto* plugin_content_frame_view =
                  FindWebViewPluginContentFrameView(*layout_embedded_content))
            Walk(*plugin_content_frame_view, context);
        } else {
          // We need to do nothing for RemoteFrameView. See crbug.com/579281.
        }
      }
    }
  }
  if (!pre_paint_info || pre_paint_info->is_last_for_node)
    object.GetMutableForPainting().ClearPaintFlags();
}

}  // namespace blink
