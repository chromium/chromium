// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_layer_property_updater.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void PrePaintTreeWalk::WalkTree(LocalFrameView& root_frame_view) {
  if (root_frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  DCHECK(root_frame_view.GetFrame().GetDocument()->Lifecycle().GetState() ==
         DocumentLifecycle::kInPrePaint);

  // Reserve 50 elements for a really deep DOM. If the nesting is deeper than
  // this, then the vector will reallocate, but it shouldn't be a big deal. This
  // is also temporary within this function.
  DCHECK_EQ(context_storage_.size(), 0u);
  context_storage_.ReserveCapacity(50);
  context_storage_.emplace_back();

  // GeometryMapper depends on paint properties.
  bool needs_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(root_frame_view, context_storage_.back());
  if (needs_tree_builder_context_update)
    GeometryMapper::ClearCache();

  if (root_frame_view.GetFrame().IsMainFrame()) {
    auto property_changed = VisualViewportPaintPropertyTreeBuilder::Update(
        root_frame_view.GetPage()->GetVisualViewport(),
        *context_storage_.back().tree_builder_context);

    if (property_changed >
        PaintPropertyChangeType::kChangedOnlyCompositedValues) {
      root_frame_view.SetPaintArtifactCompositorNeedsUpdate();
    }
  }

  Walk(root_frame_view);
  paint_invalidator_.ProcessPendingDelayedPaintInvalidations();
  context_storage_.pop_back();

#if DCHECK_IS_ON()
  if (needs_tree_builder_context_update) {
    if (VLOG_IS_ON(2) && root_frame_view.GetLayoutView()) {
      LOG(ERROR) << "PrePaintTreeWalk::Walk(root_frame_view="
                 << &root_frame_view << ")\nPaintLayer tree:";
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
      client->InvalidateRect(IntRect(IntPoint(), root_frame_view.Size()));
  }
}

void PrePaintTreeWalk::Walk(LocalFrameView& frame_view) {
  if (frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  // We need to be careful not to have a reference to the parent context, since
  // this reference will be to the context_storage_ memory which may be
  // reallocated during this function call.
  wtf_size_t parent_context_index = context_storage_.size() - 1;
  auto parent_context = [this,
                         parent_context_index]() -> PrePaintTreeWalkContext& {
    return context_storage_[parent_context_index];
  };

  bool needs_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(frame_view, parent_context());

  // Note that because we're emplacing an object constructed from
  // parent_context() (which is a reference to the vector itself), it's
  // important to first ensure that there's sufficient capacity in the vector.
  // Otherwise, it may reallocate causing parent_context() to point to invalid
  // memory.
  ResizeContextStorageIfNeeded();
  context_storage_.emplace_back(parent_context(),
                                PaintInvalidatorContext::ParentContextAccessor(
                                    this, parent_context_index),
                                needs_tree_builder_context_update);
  auto context = [this]() -> PrePaintTreeWalkContext& {
    return context_storage_.back();
  };

  // ancestor_overflow_paint_layer does not cross frame boundaries.
  context().ancestor_overflow_paint_layer = nullptr;
  if (context().tree_builder_context) {
    PaintPropertyTreeBuilder::SetupContextForFrame(
        frame_view, *context().tree_builder_context);
    context().tree_builder_context->supports_composited_raster_invalidation =
        frame_view.GetFrame().GetSettings()->GetAcceleratedCompositingEnabled();
  }

  if (LayoutView* view = frame_view.GetLayoutView()) {
#if DCHECK_IS_ON()
    if (VLOG_IS_ON(3) && needs_tree_builder_context_update) {
      LOG(ERROR) << "PrePaintTreeWalk::Walk(frame_view=" << &frame_view
                 << ")\nLayout tree:";
      showLayoutTree(view);
    }
#endif

    Walk(*view);
#if DCHECK_IS_ON()
    view->AssertSubtreeClearedPaintInvalidationFlags();
#endif
  }

  frame_view.GetLayoutShiftTracker().NotifyPrePaintFinished();
  context_storage_.pop_back();
}

bool PrePaintTreeWalk::NeedsEffectiveAllowedTouchActionUpdate(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) const {
  return context.effective_allowed_touch_action_changed ||
         object.EffectiveAllowedTouchActionChanged() ||
         object.DescendantEffectiveAllowedTouchActionChanged();
}

namespace {
bool HasBlockingTouchEventHandler(const LocalFrame& frame,
                                  EventTarget& target) {
  if (!target.HasEventListeners())
    return false;
  const auto& registry = frame.GetEventHandlerRegistry();
  const auto* blocking = registry.EventHandlerTargets(
      EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  const auto* blocking_low_latency = registry.EventHandlerTargets(
      EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency);
  return blocking->Contains(&target) || blocking_low_latency->Contains(&target);
}

bool HasBlockingTouchEventHandler(const LayoutObject& object) {
  if (object.IsLayoutView()) {
    auto* frame = object.GetFrame();
    if (HasBlockingTouchEventHandler(*frame, *frame->DomWindow()))
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
  return HasBlockingTouchEventHandler(*object.GetFrame(), *node);
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

void PrePaintTreeWalk::InvalidatePaintForHitTesting(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (context.paint_invalidator_context.subtree_flags &
      PaintInvalidatorContext::kSubtreeNoInvalidation)
    return;

  if (!context.effective_allowed_touch_action_changed)
    return;

  context.paint_invalidator_context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidator(object).InvalidateDisplayItemClient(
      object, PaintInvalidationReason::kHitTest);
}

void PrePaintTreeWalk::UpdateAuxiliaryObjectProperties(
    const LayoutObject& object,
    PrePaintTreeWalk::PrePaintTreeWalkContext& context) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (!object.HasLayer())
    return;

  PaintLayer* paint_layer = ToLayoutBoxModelObject(object).Layer();
  paint_layer->UpdateAncestorOverflowLayer(
      context.ancestor_overflow_paint_layer);

  if (object.StyleRef().HasStickyConstrainedPosition()) {
    paint_layer->GetLayoutObject().UpdateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect the
    // sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer (i.e.
    // update layer position and ancestor inputs updates in the same walk).
    paint_layer->UpdateLayerPosition();
  }
  if (paint_layer->IsRootLayer() || object.HasOverflowClip())
    context.ancestor_overflow_paint_layer = paint_layer;
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LocalFrameView& frame_view,
    const PrePaintTreeWalkContext& context) {
  if (frame_view.GetFrame().IsMainFrame() &&
      frame_view.GetPage()->GetVisualViewport().NeedsPaintPropertyUpdate()) {
    return true;
  }

  return frame_view.GetLayoutView() &&
         (ObjectRequiresTreeBuilderContext(*frame_view.GetLayoutView()) ||
          ContextRequiresTreeBuilderContext(context,
                                            *frame_view.GetLayoutView()));
}

bool PrePaintTreeWalk::ObjectRequiresPrePaint(const LayoutObject& object) {
  return object.ShouldCheckForPaintInvalidation() ||
         object.EffectiveAllowedTouchActionChanged() ||
         object.DescendantEffectiveAllowedTouchActionChanged();
}

bool PrePaintTreeWalk::ContextRequiresPrePaint(
    const PrePaintTreeWalkContext& context) {
  return context.paint_invalidator_context.NeedsSubtreeWalk() ||
         context.effective_allowed_touch_action_changed || context.clip_changed;
}

bool PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(
    const LayoutObject& object) {
  return object.NeedsPaintPropertyUpdate() ||
         (!object.PrePaintBlockedByDisplayLock(
              DisplayLockLifecycleTarget::kChildren) &&
          (object.DescendantNeedsPaintPropertyUpdate() ||
           object.DescendantNeedsPaintOffsetAndVisualRectUpdate()));
}

bool PrePaintTreeWalk::ContextRequiresTreeBuilderContext(
    const PrePaintTreeWalkContext& context,
    const LayoutObject& object) {
  return (context.tree_builder_context &&
          context.tree_builder_context->force_subtree_update_reasons) ||
         context.paint_invalidator_context.NeedsVisualRectUpdate(object);
}

void PrePaintTreeWalk::CheckTreeBuilderContextState(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& parent_context) {
  if (parent_context.tree_builder_context ||
      (!ObjectRequiresTreeBuilderContext(object) &&
       !ContextRequiresTreeBuilderContext(parent_context, object))) {
    return;
  }

  CHECK(!object.NeedsPaintPropertyUpdate());
  CHECK(!object.DescendantNeedsPaintPropertyUpdate());
  CHECK(!object.DescendantNeedsPaintOffsetAndVisualRectUpdate());
  if (parent_context.paint_invalidator_context.NeedsVisualRectUpdate(object)) {
    // Note that if paint_invalidator_context's NeedsVisualRectUpdate(object) is
    // true, we definitely want to CHECK. However, we would also like to know
    // the value of object.NeedsPaintOffsetAndVisualRectUpdate(), hence one of
    // the two CHECKs below will definitely trigger, and depending on which one
    // does we will know the value.
    CHECK(object.NeedsPaintOffsetAndVisualRectUpdate());
    CHECK(!object.NeedsPaintOffsetAndVisualRectUpdate());
  }
  CHECK(false) << "Unknown reason.";
}

void PrePaintTreeWalk::WalkInternal(const LayoutObject& object,
                                    PrePaintTreeWalkContext& context) {
  PaintInvalidatorContext& paint_invalidator_context =
      context.paint_invalidator_context;

  // This must happen before updatePropertiesForSelf, because the latter reads
  // some of the state computed here.
  UpdateAuxiliaryObjectProperties(object, context);

  base::Optional<PaintPropertyTreeBuilder> property_tree_builder;
  PaintPropertyChangeType property_changed =
      PaintPropertyChangeType::kUnchanged;
  if (context.tree_builder_context) {
    property_tree_builder.emplace(object, *context.tree_builder_context);
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
  // depends on the effective allowed touch action.
  UpdateEffectiveAllowedTouchAction(object, context);

  if (paint_invalidator_.InvalidatePaint(
          object, base::OptionalOrNullptr(context.tree_builder_context),
          paint_invalidator_context))
    needs_invalidate_chrome_client_ = true;

  InvalidatePaintForHitTesting(object, context);

  if (context.tree_builder_context) {
    property_changed =
        std::max(property_changed, property_tree_builder->UpdateForChildren());

    // Save clip_changed flag in |context| so that all descendants will see it
    // even if we don't create tree_builder_context.
    if (context.tree_builder_context->clip_changed)
      context.clip_changed = true;

    if (property_changed != PaintPropertyChangeType::kUnchanged) {
      if (property_changed >
          PaintPropertyChangeType::kChangedOnlyCompositedValues) {
        object.GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
      }

      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        if (property_changed >
            PaintPropertyChangeType::kChangedOnlyCompositedValues) {
          const auto* paint_invalidation_layer =
              paint_invalidator_context.paint_invalidation_container->Layer();
          if (!paint_invalidation_layer->SelfNeedsRepaint()) {
            auto* mapping =
                paint_invalidation_layer->GetCompositedLayerMapping();
            if (!mapping)
              mapping = paint_invalidation_layer->GroupedMapping();
            if (mapping)
              mapping->SetNeedsCheckRasterInvalidation();
          }
        }
      } else if (!context.tree_builder_context
                      ->supports_composited_raster_invalidation) {
        paint_invalidator_context.subtree_flags |=
            PaintInvalidatorContext::kSubtreeFullInvalidation;
      }
    }
  }

  // When this or ancestor clip changed, the layer needs repaint because it
  // may paint more or less results according to the changed clip.
  if (context.clip_changed && object.HasLayer())
    ToLayoutBoxModelObject(object).Layer()->SetNeedsRepaint();

  CompositingLayerPropertyUpdater::Update(object);
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

void PrePaintTreeWalk::Walk(const LayoutObject& object) {
  // We need to be careful not to have a reference to the parent context, since
  // this reference will be to the context_storage_ memory which may be
  // reallocated during this function call.
  wtf_size_t parent_context_index = context_storage_.size() - 1;
  auto parent_context = [this,
                         parent_context_index]() -> PrePaintTreeWalkContext& {
    return context_storage_[parent_context_index];
  };

  bool needs_tree_builder_context_update =
      ContextRequiresTreeBuilderContext(parent_context(), object) ||
      ObjectRequiresTreeBuilderContext(object);

  // The following is for debugging crbug.com/974639.
  CheckTreeBuilderContextState(object, parent_context());

  // Early out from the tree walk if possible.
  if (!needs_tree_builder_context_update && !ObjectRequiresPrePaint(object) &&
      !ContextRequiresPrePaint(parent_context())) {
    return;
  }

  // Note that because we're emplacing an object constructed from
  // parent_context() (which is a reference to the vector itself), it's
  // important to first ensure that there's sufficient capacity in the vector.
  // Otherwise, it may reallocate causing parent_context() to point to invalid
  // memory.
  ResizeContextStorageIfNeeded();
  context_storage_.emplace_back(parent_context(),
                                PaintInvalidatorContext::ParentContextAccessor(
                                    this, parent_context_index),
                                needs_tree_builder_context_update);
  auto context = [this]() -> PrePaintTreeWalkContext& {
    return context_storage_.back();
  };

  // Ignore clip changes from ancestor across transform boundaries.
  if (object.StyleRef().HasTransform()) {
    context().clip_changed = false;
    if (context().tree_builder_context)
      context().tree_builder_context->clip_changed = false;
  }

  WalkInternal(object, context());
  object.NotifyDisplayLockDidPrePaint(DisplayLockLifecycleTarget::kSelf);

  bool child_walk_blocked = object.PrePaintBlockedByDisplayLock(
      DisplayLockLifecycleTarget::kChildren);
  // If we need a subtree walk due to context flags, we need to store that
  // information on the display lock, since subsequent walks might not set the
  // same bits on the context.
  if (child_walk_blocked &&
      (ContextRequiresTreeBuilderContext(context(), object) ||
       ContextRequiresPrePaint(context()))) {
    // Note that effective allowed touch action changed is special in that
    // it requires us to specifically recalculate this value on each subtree
    // element. Other flags simply need a subtree walk. Some consideration
    // needs to be given to |clip_changed| which ensures that we repaint every
    // layer, but for the purposes of PrePaint, this flag is just forcing a
    // subtree walk.
    object.GetDisplayLockContext()->SetNeedsPrePaintSubtreeWalk(
        context().effective_allowed_touch_action_changed);
  }

  if (!child_walk_blocked) {
    for (const LayoutObject* child = object.SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsLayoutMultiColumnSpannerPlaceholder()) {
        child->GetMutableForPainting().ClearPaintFlags();
        continue;
      }
      Walk(*child);
    }

    if (object.IsLayoutEmbeddedContent()) {
      const LayoutEmbeddedContent& layout_embedded_content =
          ToLayoutEmbeddedContent(object);
      if (auto* embedded_view =
              layout_embedded_content.GetEmbeddedContentView()) {
        if (context().tree_builder_context) {
          auto& offset =
              context().tree_builder_context->fragments[0].current.paint_offset;
          offset += layout_embedded_content.ReplacedContentRect().offset;
          offset -= PhysicalOffset(embedded_view->FrameRect().Location());
          offset = PhysicalOffset(RoundedIntPoint(offset));
        }
        if (embedded_view->IsLocalFrameView()) {
          Walk(*To<LocalFrameView>(embedded_view));
        } else if (embedded_view->IsPluginView()) {
          // If it is a webview plugin, walk into the content frame view.
          if (auto* plugin_content_frame_view =
                  FindWebViewPluginContentFrameView(layout_embedded_content))
            Walk(*plugin_content_frame_view);
        } else {
          // We need to do nothing for RemoteFrameView. See crbug.com/579281.
        }
      }
    }

    object.NotifyDisplayLockDidPrePaint(DisplayLockLifecycleTarget::kChildren);
  }

  object.GetMutableForPainting().ClearPaintFlags();
  context_storage_.pop_back();
}

void PrePaintTreeWalk::ResizeContextStorageIfNeeded() {
  if (UNLIKELY(context_storage_.size() == context_storage_.capacity())) {
    DCHECK_GT(context_storage_.size(), 0u);
    context_storage_.ReserveCapacity(context_storage_.size() * 2);
  }
}

}  // namespace blink
