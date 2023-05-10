// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/pre_paint_disable_side_effects_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool IsLinkHighlighted(const LayoutObject& object) {
  return object.GetFrame()->GetPage()->GetLinkHighlight().IsHighlighting(
      object);
}

}  // anonymous namespace

bool PrePaintTreeWalk::ContainingFragment::IsInFragmentationContext() const {
  return fragment && fragment->IsFragmentainerBox();
}

void PrePaintTreeWalk::WalkTree(LocalFrameView& root_frame_view) {
  if (root_frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  DCHECK_EQ(root_frame_view.GetFrame().GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);

  PrePaintTreeWalkContext context;

#if DCHECK_IS_ON()
  bool needed_tree_builder_context_update =
      NeedsTreeBuilderContextUpdate(root_frame_view, context);
#endif

  VisualViewport& visual_viewport =
      root_frame_view.GetPage()->GetVisualViewport();
  if (visual_viewport.IsActiveViewport() &&
      root_frame_view.GetFrame().IsMainFrame()) {
    VisualViewportPaintPropertyTreeBuilder::Update(
        root_frame_view, visual_viewport, *context.tree_builder_context);
  }

  Walk(root_frame_view, context);
  paint_invalidator_.ProcessPendingDelayedPaintInvalidations();

#if DCHECK_IS_ON()
  if (needed_tree_builder_context_update && VLOG_IS_ON(1)) {
    ShowAllPropertyTrees(root_frame_view);
  }
#endif

  bool updates_executed = root_frame_view.ExecuteAllPendingUpdates();
  if (updates_executed) {
    needs_invalidate_chrome_client_ = true;
  }

  // If the page has anything changed, we need to inform the chrome client
  // so that the client will initiate repaint of the contents if needed (e.g.
  // when this page is embedded as a non-composited content of another page).
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
  context.ResetFragmentation();

  // ancestor_scroll_container_paint_layer does not cross frame boundaries.
  context.ancestor_scroll_container_paint_layer = nullptr;
  if (context.tree_builder_context) {
    PaintPropertyTreeBuilder::SetupContextForFrame(
        frame_view, *context.tree_builder_context);
  }

  if (LayoutView* view = frame_view.GetLayoutView()) {
#if DCHECK_IS_ON()
    if (VLOG_IS_ON(3) && needs_tree_builder_context_update) {
      VLOG(3) << "PrePaintTreeWalk::Walk(frame_view=" << &frame_view
              << ")\nLayout tree:";
      ShowLayoutTree(view);
      VLOG(3) << "Fragment tree:";
      ShowFragmentTree(*view);
    }
#endif
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

  if (auto* node = object.GetNode()) {
    return HasBlockingEventHandlerHelper(*object.GetFrame(), *node, event_type);
  }

  return false;
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
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LocalFrameView& frame_view,
    const PrePaintTreeWalkContext& context) {
  if (frame_view.GetFrame().IsMainFrame() &&
      frame_view.GetPage()->GetVisualViewport().IsActiveViewport() &&
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
}

bool PrePaintTreeWalk::ContextRequiresChildPrePaint(
    const PrePaintTreeWalkContext& context) {
  return context.paint_invalidator_context.NeedsSubtreeWalk() ||
         context.effective_allowed_touch_action_changed ||
         context.blocking_wheel_event_handler_changed;
}

bool PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(
    const LayoutObject& object) {
  return object.NeedsPaintPropertyUpdate() ||
         object.ShouldCheckLayoutForPaintInvalidation() ||
         (!object.ChildPrePaintBlockedByDisplayLock() &&
          (object.DescendantNeedsPaintPropertyUpdate() ||
           object.DescendantShouldCheckLayoutForPaintInvalidation()));
}

bool PrePaintTreeWalk::ContextRequiresChildTreeBuilderContext(
    const PrePaintTreeWalkContext& context) {
  if (!context.NeedsTreeBuilderContext()) {
    DCHECK(!context.tree_builder_context ||
           !context.tree_builder_context->force_subtree_update_reasons);
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
  DCHECK(!object.DescendantShouldCheckLayoutForPaintInvalidation());
  DCHECK(!object.ShouldCheckLayoutForPaintInvalidation());
  NOTREACHED() << "Unknown reason.";
}
#endif

NGPrePaintInfo PrePaintTreeWalk::CreatePrePaintInfo(
    const NGLink& child,
    const PrePaintTreeWalkContext& context) {
  const auto* fragment = To<NGPhysicalBoxFragment>(child.fragment.Get());
  return NGPrePaintInfo(fragment, child.offset,
                        context.current_container.fragmentainer_idx,
                        fragment->IsFirstForNode(), !fragment->BreakToken(),
                        /* is_inside_fragment_child */ false,
                        context.current_container.IsInFragmentationContext());
}

FragmentData* PrePaintTreeWalk::GetOrCreateFragmentData(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& context,
    const NGPrePaintInfo& pre_paint_info) {
  // If |allow_update| is set, we're allowed to add, remove and modify
  // FragmentData objects. Otherwise they will be left alone.
  bool allow_update = context.NeedsTreeBuilderContext();

  FragmentData* fragment_data = &object.GetMutableForPainting().FirstFragment();

  // BR elements never fragment. While there are parts of the code that depend
  // on the correct paint offset (getBoundingClientRect(), etc.), we don't need
  // to set fragmentation info (nor create multiple FragmentData entries). BR
  // elements aren't necessarily marked for invalidation when laid out (which
  // means that allow_update won't be set when it should, and the code below
  // would get confused).
  if (object.IsBR())
    return fragment_data;

  // The need for paint properties is the same across all fragments, so if the
  // first FragmentData needs it, so do all the others.
  bool needs_paint_properties = fragment_data->PaintProperties();

  wtf_size_t fragment_id = pre_paint_info.fragmentainer_idx;
  if (pre_paint_info.is_first_for_node) {
    if (allow_update) {
      if (fragment_data->FragmentID() < fragment_id) {
        fragment_data->ClearNextFragment();
      } else {
        // We're at the first fragment. Mark all additional FragmentData
        // objects, so that we can tell that they have been kept from a previous
        // pre-paint pass.
        for (FragmentData* next = fragment_data->NextFragment(); next;
             next = next->NextFragment())
          next->SetNeedsUpdate(true);
      }
    }
  } else {
    FragmentData* last_fragment = nullptr;
    // If fragment_data->NeedsUpdate() is true, a fragment ID mismatch is
    // possible. Otherwise just loop through the FragmentData entries until we
    // find the matching ID (or reach the end). The IDs are in ascending order,
    // but they may not always be contiguous, as some nodes may lack a fragment
    // representation certain fragmentainers.
    do {
      if (fragment_data->FragmentID() == fragment_id)
        break;
      if (fragment_data->NeedsUpdate()) {
        // Fragment ID mismatch. In some cases (typically when out-of-flow
        // layout inserts fragmentainers on its own) we might skip a container
        // in a given fragmentainer. We can re-use this FragmentData entry and
        // just update the fragment ID. The important thing here is that we stop
        // even if the ID is lower than what we're looking for.
        DCHECK(allow_update);
        break;
      }
      DCHECK_LT(fragment_data->FragmentID(), fragment_id);
      last_fragment = fragment_data;
      fragment_data = fragment_data->NextFragment();
    } while (fragment_data);

    if (!fragment_data) {
      // We don't need any additional fragments for culled inlines - unless this
      // is the highlighted link (in which case even culled inlines get paint
      // effects).
      if (!object.IsBox() && !object.HasInlineFragments() &&
          !IsLinkHighlighted(object))
        return nullptr;

      DCHECK(allow_update);

      // When we add FragmentData entries, we need to make sure that we update
      // paint properties. The object may not have been marked for an update, if
      // the reason for creating an additional FragmentData was that the
      // fragmentainer block-size shrunk, for instance.
      if (!last_fragment->NextFragment())
        object.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
      fragment_data = &last_fragment->EnsureNextFragment();
    }
  }

  if (pre_paint_info.is_last_for_node) {
    // We have reached the end. There may be more data entries that were
    // needed in the previous layout, but not any more. Clear them.
    if (allow_update)
      fragment_data->ClearNextFragment();
    else
      DCHECK(!fragment_data->NextFragment());
  }

  if (allow_update) {
    fragment_data->SetNeedsUpdate(false);
    fragment_data->SetFragmentID(fragment_id);
    if (needs_paint_properties)
      fragment_data->EnsurePaintProperties();
  } else {
    DCHECK(!fragment_data->NeedsUpdate());
    DCHECK_EQ(fragment_data->FragmentID(), fragment_id);
    DCHECK(!needs_paint_properties || fragment_data->PaintProperties());
  }

  return fragment_data;
}

void PrePaintTreeWalk::UpdateContextForOOFContainer(
    const LayoutObject& object,
    PrePaintTreeWalkContext& context,
    const NGPhysicalBoxFragment* fragment) {
  // Flow threads don't exist, as far as LayoutNG is concerned. Yet, we
  // encounter them here when performing an NG fragment accompanied LayoutObject
  // subtree walk. Just ignore.
  if (object.IsLayoutFlowThread())
    return;

  // If we're in a fragmentation context, the parent fragment of OOFs is the
  // fragmentainer, unless the object is monolithic, in which case nothing
  // contained by the object participates in the current block fragmentation
  // context. If we're not participating in block fragmentation, the containing
  // fragment of an OOF fragment is always simply the parent.
  if (!context.current_container.IsInFragmentationContext() ||
      (fragment && fragment->IsMonolithic())) {
    // Anonymous blocks are not allowed to be containing blocks, so we should
    // skip over any such elements.
    if (!fragment || !fragment->IsAnonymousBlock()) {
      context.current_container.fragment = fragment;
    }
  }

  if (!object.CanContainAbsolutePositionObjects())
    return;

  // The OOF containing block structure is special under block fragmentation: A
  // fragmentable OOF is always a direct child of a fragmentainer.
  context.absolute_positioned_container = context.current_container;
  if (object.CanContainFixedPositionObjects())
    context.fixed_positioned_container = context.absolute_positioned_container;
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
    if (!pre_paint_info->fragment_data)
      return;
  } else if (object.IsFragmentLessBox()) {
    return;
  }

  absl::optional<PaintPropertyTreeBuilder> property_tree_builder;
  if (context.tree_builder_context) {
    property_tree_builder.emplace(object, pre_paint_info,
                                  *context.tree_builder_context);
    property_tree_builder->UpdateForSelf();
  }

  // This must happen before paint invalidation because background painting
  // depends on the effective allowed touch action and blocking wheel event
  // handlers.
  UpdateEffectiveAllowedTouchAction(object, context);
  UpdateBlockingWheelEventHandler(object, context);

  if (paint_invalidator_.InvalidatePaint(
          object, pre_paint_info,
          base::OptionalToPtr(context.tree_builder_context),
          paint_invalidator_context))
    needs_invalidate_chrome_client_ = true;

  InvalidatePaintForHitTesting(object, context);

  if (context.tree_builder_context) {
    property_tree_builder->UpdateForChildren();
    property_tree_builder->IssueInvalidationsAfterUpdate();
    needs_invalidate_chrome_client_ |=
        property_tree_builder->PropertiesChanged();
  }
}

bool PrePaintTreeWalk::CollectMissableChildren(
    PrePaintTreeWalkContext& context,
    const NGPhysicalBoxFragment& parent) {
  bool has_missable_children = false;
  for (const NGLink& child : parent.Children()) {
    if (UNLIKELY(child->IsLayoutObjectDestroyedOrMoved()))
      continue;
    if ((child->IsOutOfFlowPositioned() &&
         (context.current_container.fragment || child->IsFixedPositioned())) ||
        (child->IsFloating() && parent.IsInlineFormattingContext() &&
         context.current_container.fragment)) {
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

const NGPhysicalBoxFragment*
PrePaintTreeWalk::RebuildContextForMissedDescendant(
    const NGPhysicalBoxFragment& ancestor,
    const LayoutObject& object,
    bool update_tree_builder_context,
    PrePaintTreeWalkContext& context) {
  // Walk up to the ancestor and, on the way down again, adjust the context with
  // info about OOF containing blocks.
  if (&object == ancestor.OwnerLayoutBox()) {
    return &ancestor;
  }
  const NGPhysicalBoxFragment* search_fragment =
      RebuildContextForMissedDescendant(ancestor, *object.Parent(),
                                        update_tree_builder_context, context);

  if (object.IsLayoutFlowThread()) {
    // A flow threads doesn't create fragments. Just ignore it.
    return search_fragment;
  }

  const NGPhysicalBoxFragment* box_fragment = nullptr;
  if (context.tree_builder_context && update_tree_builder_context) {
    PhysicalOffset paint_offset;
    wtf_size_t fragmentainer_idx = context.current_container.fragmentainer_idx;

    // TODO(mstensho): We're doing a simplified version of what
    // WalkLayoutObjectChildren() does. Consider refactoring so that we can
    // share.
    if (object.IsOutOfFlowPositioned()) {
      // The fragment tree follows the structure of containing blocks closely,
      // while here we're walking down the LayoutObject tree spine (which
      // follows the structure of the flat DOM tree, more or less). This means
      // that for out-of-flow positioned objects, the fragment of the parent
      // LayoutObject might not be the right place to search.
      const ContainingFragment& oof_containing_fragment_info =
          object.IsFixedPositioned() ? context.fixed_positioned_container
                                     : context.absolute_positioned_container;
      search_fragment = oof_containing_fragment_info.fragment;
      fragmentainer_idx = oof_containing_fragment_info.fragmentainer_idx;
    }
    // If we have a parent fragment to search inside, do that. If we find it, we
    // can use its paint offset and size in the paint property builder. If we
    // have no parent fragment, or don't find the child, we won't be passing a
    // fragment to the property builder, and then it needs to behave
    // accordingly, e.g. assume that the fragment is at the fragmentainer
    // origin, and has zero block-size.
    // See e.g. https://www.w3.org/TR/css-break-3/#transforms
    if (search_fragment) {
      for (NGLink link : search_fragment->Children()) {
        if (link->GetLayoutObject() == object) {
          box_fragment = To<NGPhysicalBoxFragment>(link.get());
          paint_offset = link.offset;
          break;
        }
      }
    }

    // TODO(mstensho): Some of the bool parameters here are meaningless when
    // only used with PaintPropertyTreeBuilder (only used by
    // PrePaintTreeWalker). Consider cleaning this up, by splitting up
    // NGPrePaintInfo into one walker part and one builder part, so that we
    // don't have to specify them as false here.
    NGPrePaintInfo pre_paint_info(
        box_fragment, paint_offset, fragmentainer_idx,
        /* is_first_for_node */ false, /* is_last_for_node */ false,
        /* is_inside_fragment_child */ false,
        context.current_container.IsInFragmentationContext());

    // We're going to set up paint properties for the missing ancestors, and
    // update the context, but it should have no side-effects. That is, the
    // LayoutObject(s) should be left untouched. PaintPropertyTreeBuilder
    // normally calls LayoutObject::GetMutableForPainting() and does stuff, but
    // we need to avoid that in this case.
    PrePaintDisableSideEffectsScope leave_layout_object_alone_kthanksbye;

    // Also just create a dummy FragmentData object. We don't want any
    // side-effect, but the paint property tree builder requires a FragmentData
    // object to write stuff into.
    pre_paint_info.fragment_data = MakeGarbageCollected<FragmentData>();

    PaintPropertyTreeBuilderContext& builder_context =
        context.tree_builder_context.value();
    auto original_force_update = builder_context.force_subtree_update_reasons;
    // Since we're running without any old paint properties (since we're passing
    // a dummy FragmentData object), we need to recalculate all properties.
    builder_context.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;

    PaintPropertyTreeBuilder property_tree_builder(object, &pre_paint_info,
                                                   builder_context);
    property_tree_builder.UpdateForSelf();
    property_tree_builder.UpdateForChildren();
    builder_context.force_subtree_update_reasons = original_force_update;
  }

  UpdateContextForOOFContainer(object, context, box_fragment);

  if (!object.CanContainAbsolutePositionObjects() ||
      !context.tree_builder_context) {
    return box_fragment;
  }

  PaintPropertyTreeBuilderContext& property_context =
      *context.tree_builder_context;
  PaintPropertyTreeBuilderFragmentContext& fragment_context =
      property_context.fragments[0];
  // Reset the relevant OOF context to this fragmentainer, since this is its
  // containing block, as far as the NG fragment structure is concerned.
  property_context.container_for_absolute_position = &object;
  fragment_context.absolute_position = fragment_context.current;
  if (object.CanContainFixedPositionObjects()) {
    property_context.container_for_fixed_position = &object;
    fragment_context.fixed_position = fragment_context.current;
  }

  return box_fragment;
}

void PrePaintTreeWalk::WalkMissedChildren(
    const NGPhysicalBoxFragment& fragment,
    const PrePaintTreeWalkContext& context) {
  if (pending_missables_.empty())
    return;

  // Missing fragments are assumed to be at the start block edge of the
  // fragmentainer. When generating fragments, layout sets their correct
  // block-offset (obviously), as a physical offset. But since we're just
  // pretending to have a fragment in this case, we have to do it ourselves. For
  // vertical-rl, the block-start offset is at the right edge of the
  // fragmentainer, not at the left (vertical-lr) (which is zero), and not at
  // the top (horizontal-tb) (also zero). So we need to adjust for vertical-rl.
  PhysicalOffset offset_to_block_start_edge;
  if (fragment.IsFragmentainerBox() &&
      fragment.Style().GetWritingMode() == WritingMode::kVerticalRl) {
    offset_to_block_start_edge.left = fragment.Size().width;
  }

  for (const NGLink& child : fragment.Children()) {
    if (UNLIKELY(child->IsLayoutObjectDestroyedOrMoved()))
      continue;
    if (!child->IsOutOfFlowPositioned() && !child->IsFloating())
      continue;
    if (!pending_missables_.Contains(child.fragment))
      continue;
    const LayoutObject& descendant_object = *child->GetLayoutObject();
    PrePaintTreeWalkContext descendant_context(
        context, NeedsTreeBuilderContextUpdate(descendant_object, context));
    if (child->IsOutOfFlowPositioned()) {
      if (descendant_context.tree_builder_context.has_value()) {
        PaintPropertyTreeBuilderContext* builder_context =
            &descendant_context.tree_builder_context.value();
        builder_context->fragments[0].current.paint_offset +=
            offset_to_block_start_edge;
      }

      bool update_tree_builder_context =
          RuntimeEnabledFeatures::PrePaintAncestorsOfMissedOOFEnabled() &&
          NeedsTreeBuilderContextUpdate(descendant_object, descendant_context);

      RebuildContextForMissedDescendant(fragment, *descendant_object.Parent(),
                                        update_tree_builder_context,
                                        descendant_context);
    }

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
    const PrePaintTreeWalkContext& parent_context) {
  // If this is a multicol container, the actual children are inside the flow
  // thread child of |object|.
  const auto* flow_thread =
      To<LayoutBlockFlow>(&object)->MultiColumnFlowThread();
  const LayoutObject& actual_parent = flow_thread ? *flow_thread : object;

  DCHECK(fragment.IsFragmentationContextRoot());

  absl::optional<wtf_size_t> inner_fragmentainer_idx;

  for (NGLink child : fragment.Children()) {
    const auto* box_fragment = To<NGPhysicalBoxFragment>(child.fragment.Get());
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

      NGPrePaintInfo pre_paint_info = CreatePrePaintInfo(child, parent_context);
      Walk(*box_fragment->GetLayoutObject(), parent_context, &pre_paint_info);
      continue;
    }

    // Check |box_fragment| and the |LayoutBox| that produced it are in sync.
    // |OwnerLayoutBox()| has a few DCHECKs for this purpose.
    DCHECK(box_fragment->OwnerLayoutBox());

    // A fragmentainer doesn't paint anything itself. Just include its offset
    // and descend into children.
    DCHECK(box_fragment->IsFragmentainerBox());

    PrePaintTreeWalkContext fragmentainer_context(
        parent_context, parent_context.NeedsTreeBuilderContext());

    fragmentainer_context.current_container.fragmentation_nesting_level++;
    fragmentainer_context.is_parent_first_for_node =
        box_fragment->IsFirstForNode();

    // Always keep track of the current innermost fragmentainer we're handling,
    // as they may serve as containing blocks for OOF descendants.
    fragmentainer_context.current_container.fragment = box_fragment;

    // Set up |inner_fragmentainer_idx| lazily, as it's O(n) (n == number of
    // multicol container fragments).
    if (!inner_fragmentainer_idx)
      inner_fragmentainer_idx = PreviousInnerFragmentainerIndex(fragment);
    fragmentainer_context.current_container.fragmentainer_idx =
        *inner_fragmentainer_idx;

    PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext*
        containing_block_context = nullptr;
    if (LIKELY(fragmentainer_context.tree_builder_context)) {
      PaintPropertyTreeBuilderFragmentContext& fragment_context =
          fragmentainer_context.tree_builder_context->fragments[0];
      containing_block_context = &fragment_context.current;
      containing_block_context->paint_offset += child.offset;

      // Keep track of the paint offset at the fragmentainer. This is needed
      // when entering OOF descendants. OOFs have the nearest fragmentainer as
      // their containing block, so when entering them during LayoutObject tree
      // traversal, we have to compensate for this.
      containing_block_context->paint_offset_for_oof_in_fragmentainer =
          containing_block_context->paint_offset;

      if (object.IsLayoutView()) {
        // Out-of-flow positioned descendants are positioned relatively to this
        // fragmentainer (page).
        fragment_context.absolute_position = *containing_block_context;
        fragment_context.fixed_position = *containing_block_context;
      }
    }

    WalkChildren(actual_parent, box_fragment, fragmentainer_context);

    if (containing_block_context)
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
    const PrePaintTreeWalkContext& context) {
  absl::optional<NGInlineCursor> inline_cursor;
  for (const LayoutObject* child = parent_object.SlowFirstChild(); child;
       // Stay on the |child| while iterating fragments of |child|.
       child = inline_cursor ? child : child->NextSibling()) {
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
    wtf_size_t fragmentainer_idx = context.current_container.fragmentainer_idx;
    const ContainingFragment* oof_containing_fragment_info = nullptr;
    PhysicalOffset paint_offset;
    const auto* child_box = DynamicTo<LayoutBox>(child);
    bool is_first_for_node = true;
    bool is_last_for_node = true;
    bool is_inside_fragment_child = false;

    if (!inline_cursor && parent_fragment->HasItems() &&
        child->HasInlineFragments()) {
      // Limit the search to descendants of |parent_fragment|.
      inline_cursor.emplace(*parent_fragment);
      inline_cursor->MoveTo(*child);
      // Searching fragments of |child| may not find any because they may be in
      // other fragmentainers than |parent_fragment|.
    }
    if (inline_cursor) {
      for (; inline_cursor->Current();
           inline_cursor->MoveToNextForSameLayoutObject()) {
        // Check if the search is limited to descendants of |parent_fragment|.
        DCHECK_EQ(&inline_cursor->ContainerFragment(), parent_fragment);
        const NGFragmentItem& item = *inline_cursor->Current().Item();
        DCHECK_EQ(item.GetLayoutObject(), child);

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
      if (is_last_for_node || !inline_cursor->Current()) {
        // If all fragments are done, move to the next sibling of |child|.
        inline_cursor.reset();
      } else {
        inline_cursor->MoveToNextForSameLayoutObject();
      }
      if (!box_fragment)
        continue;
    } else if (child->IsInline() && !child_box) {
      // This child is a non-atomic inline (or text), but we have no cursor.
      // The cursor will be missing if the child has no fragment representation,
      // or if the container has no fragment items (which may happen if there's
      // only collapsed text / culled inlines, or if we had to insert a break in
      // a block before we got to any inline content, or if the container only
      // has resumed floats).

      // If the child has a fragment representation, we're going to find it in
      // the fragmentainer(s) where it occurs.
      if (child->HasInlineFragments())
        continue;

      const auto* layout_inline_child = DynamicTo<LayoutInline>(child);

      if (!layout_inline_child) {
        // We end up here for collapsed text nodes. Just clear the paint flags.
        for (const LayoutObject* fragmentless = child; fragmentless;
             fragmentless = fragmentless->NextInPreOrder(child)) {
          DCHECK(fragmentless->IsText());
          DCHECK(!fragmentless->HasInlineFragments());
          fragmentless->GetMutableForPainting().ClearPaintFlags();
        }
        continue;
      }

      if (layout_inline_child->FirstChild()) {
        // We have to enter culled inlines for every block fragment where any of
        // their children has a representation.
        if (!parent_fragment->HasItems())
          continue;

        if (!parent_fragment->Items()->IsContainerForCulledInline(
                *layout_inline_child, &is_first_for_node, &is_last_for_node))
          continue;
      } else {
        // Childless and culled. This can happen for AREA elements, if nothing
        // else. Enter them when visiting the parent for the first time.
        if (!context.is_parent_first_for_node)
          continue;
        is_first_for_node = true;
        is_last_for_node = true;
      }

      // Inlines will pass their containing block fragment (and its incoming
      // break token).
      box_fragment = parent_fragment;
      is_inside_fragment_child = true;
    } else if (child_box && child_box->PhysicalFragmentCount()) {
      // Figure out which fragment the child may be found inside. The fragment
      // tree follows the structure of containing blocks closely, while here
      // we're walking the LayoutObject tree (which follows the structure of the
      // flat DOM tree, more or less). This means that for out-of-flow
      // positioned objects, the fragment of the parent LayoutObject might not
      // be the right place to search.
      const NGPhysicalBoxFragment* search_fragment = parent_fragment;
      if (child_box->IsOutOfFlowPositioned()) {
        oof_containing_fragment_info =
            child_box->IsFixedPositioned()
                ? &context.fixed_positioned_container
                : &context.absolute_positioned_container;
        if (context.current_container.fragmentation_nesting_level !=
            oof_containing_fragment_info->fragmentation_nesting_level) {
          // Only walk OOFs once if they aren't contained within the current
          // fragmentation context.
          if (!context.is_parent_first_for_node)
            continue;
        }

        search_fragment = oof_containing_fragment_info->fragment;
        fragmentainer_idx = oof_containing_fragment_info->fragmentainer_idx;
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
      const ContainingFragment* container_for_child =
          &context.current_container;
      bool is_in_different_fragmentation_context = false;
      if (oof_containing_fragment_info &&
          context.current_container.fragmentation_nesting_level !=
              oof_containing_fragment_info->fragmentation_nesting_level) {
        // We're walking an out-of-flow positioned descendant that isn't in the
        // same fragmentation context as parent_object. We need to update the
        // context, so that we create FragmentData objects correctly both for
        // the descendant and all its descendants.
        container_for_child = oof_containing_fragment_info;
        is_in_different_fragmentation_context = true;
      }
      NGPrePaintInfo pre_paint_info(
          box_fragment, paint_offset, fragmentainer_idx, is_first_for_node,
          is_last_for_node, is_inside_fragment_child,
          container_for_child->IsInFragmentationContext());
      if (is_in_different_fragmentation_context) {
        PrePaintTreeWalkContext oof_context(
            context, NeedsTreeBuilderContextUpdate(*child, context));
        oof_context.current_container = *container_for_child;
        Walk(*child, oof_context, &pre_paint_info);
      } else {
        Walk(*child, context, &pre_paint_info);
      }
    } else {
      Walk(*child, context, /* pre_paint_info */ nullptr);
    }
  }
}

void PrePaintTreeWalk::WalkChildren(
    const LayoutObject& object,
    const NGPhysicalBoxFragment* traversable_fragment,
    PrePaintTreeWalkContext& context,
    bool is_inside_fragment_child) {
  const LayoutBox* box = DynamicTo<LayoutBox>(&object);
  if (box) {
    if (traversable_fragment) {
      if (!box->IsLayoutFlowThread() &&
          (!box->IsLayoutNGObject() || !box->PhysicalFragmentCount())) {
        // We can traverse PhysicalFragments in LayoutMedia though it's not
        // a LayoutNGObject.
        if (!box->IsMedia()) {
          // Leave LayoutNGBoxFragment-accompanied child LayoutObject
          // traversal, since this object doesn't support that (or has no
          // fragments (happens for table columns)). We need to switch back to
          // legacy LayoutObject traversal for its children. We're then also
          // assuming that we're either not block-fragmenting, or that this is
          // monolithic content. We may re-enter
          // LayoutNGBoxFragment-accompanied traversal if we get to a
          // descendant that supports that.
          DCHECK(!box->FlowThreadContainingBlock() || box->IsMonolithic());

          traversable_fragment = nullptr;
        }
      }
    } else if (box->PhysicalFragmentCount()) {
      // Enter LayoutNGBoxFragment-accompanied child LayoutObject traversal if
      // we're at an NG fragmentation context root. While we in theory *could*
      // enter this mode for any object that has a traversable fragment, without
      // affecting correctness, we're better off with plain LayoutObject
      // traversal when possible, as fragment-accompanied traversal has O(n^2)
      // performance complexity (where n is the number of siblings).
      //
      // We'll stay in this mode for all descendants that support fragment
      // traversal. We'll re-enter legacy traversal for descendants that don't
      // support it. This only works correctly as long as there's no block
      // fragmentation in the ancestry, though, so DCHECK for that.
      DCHECK_EQ(box->PhysicalFragmentCount(), 1u);
      const auto* first_fragment =
          To<NGPhysicalBoxFragment>(box->GetPhysicalFragment(0));
      DCHECK(!first_fragment->BreakToken());
      if (first_fragment->IsFragmentationContextRoot() &&
          box->CanTraversePhysicalFragments())
        traversable_fragment = first_fragment;
    }
  }

  // Keep track of fragments that act as containers for OOFs, so that we can
  // search their children when looking for an OOF further down in the tree.
  UpdateContextForOOFContainer(object, context, traversable_fragment);

  bool has_missable_children = false;
  const NGPhysicalBoxFragment* fragment = traversable_fragment;
  if (!fragment) {
    // Even when we're not in fragment traversal mode, we need to look for
    // missable child fragments. We may enter fragment traversal mode further
    // down in the subtree, and there may be a node that's a direct child of
    // |object|, fragment-wise, while it's further down in the tree, CSS
    // box-tree-wise. This is only an issue for OOF descendants, though, so only
    // examine OOF containing blocks.
    if (box && box->CanContainAbsolutePositionObjects() &&
        box->IsLayoutNGObject() && box->PhysicalFragmentCount()) {
      DCHECK_EQ(box->PhysicalFragmentCount(), 1u);
      fragment = box->GetPhysicalFragment(0);
    }
  }
  if (fragment) {
    // If we are at a block fragment, collect any missable children.
    DCHECK(!is_inside_fragment_child || !object.IsBox());
    if (!is_inside_fragment_child)
      has_missable_children = CollectMissableChildren(context, *fragment);
  }

  // We'll always walk the LayoutObject tree when possible, but if this is a
  // fragmentation context root (such as a multicol container), we need to enter
  // each fragmentainer child and then walk all the LayoutObject children.
  if (traversable_fragment &&
      traversable_fragment->IsFragmentationContextRoot()) {
    WalkFragmentationContextRootChildren(object, *traversable_fragment,
                                         context);
  } else {
    WalkLayoutObjectChildren(object, traversable_fragment, context);
  }

  if (has_missable_children) {
    WalkMissedChildren(*fragment, context);
  }
}

void PrePaintTreeWalk::Walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parent_context,
                            NGPrePaintInfo* pre_paint_info) {
  const NGPhysicalBoxFragment* physical_fragment = nullptr;
  bool is_inside_fragment_child = false;
  if (pre_paint_info) {
    physical_fragment = pre_paint_info->box_fragment;
    DCHECK(physical_fragment);
    is_inside_fragment_child = pre_paint_info->is_inside_fragment_child;
  }

  // If we're visiting a missable fragment, remove it from the list.
  if (physical_fragment) {
    // If we're doing fragment traversal, both OOFs and floats are missable.
    if (physical_fragment->IsOutOfFlowPositioned() ||
        physical_fragment->IsFloating())
      pending_missables_.erase(physical_fragment);
  } else {
    // If we're not doing fragment traversal, only OOFs are missable.
    if (object.IsOutOfFlowPositioned()) {
      const auto& box = To<LayoutBox>(object);
      if (box.PhysicalFragmentCount()) {
        DCHECK_EQ(box.PhysicalFragmentCount(), 1u);
        pending_missables_.erase(box.GetPhysicalFragment(0));
      }
    }
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
    // flags simply need a subtree walk.
    object.GetDisplayLockContext()->SetNeedsPrePaintSubtreeWalk(
        context.effective_allowed_touch_action_changed,
        context.blocking_wheel_event_handler_changed);
  }

  if (!child_walk_blocked) {
    if (pre_paint_info)
      context.is_parent_first_for_node = pre_paint_info->is_first_for_node;

    WalkChildren(object, physical_fragment, context, is_inside_fragment_child);

    if (const auto* layout_embedded_content =
            DynamicTo<LayoutEmbeddedContent>(object)) {
      if (auto* embedded_view =
              layout_embedded_content->GetEmbeddedContentView()) {
        // Embedded content is monolithic and will normally not generate
        // multiple fragments. However, if this is inside of a repeated table
        // section or repeated fixed positioned element (printing), it may
        // generate multiple fragments. In such cases, only update when at the
        // first fragment if the underlying implementation doesn't support
        // multiple fragments. We are only going to paint/hit-test the first
        // fragment, and we need to make sure that the paint offsets inside the
        // child view are with respect to the first fragment.
        if (!physical_fragment || physical_fragment->IsFirstForNode() ||
            CanPaintMultipleFragments(*physical_fragment)) {
          if (context.tree_builder_context) {
            auto& current = context.tree_builder_context->fragments[0].current;
            current.paint_offset = PhysicalOffset(ToRoundedPoint(
                current.paint_offset +
                layout_embedded_content->ReplacedContentRect().offset -
                PhysicalOffset(embedded_view->FrameRect().origin())));
            // Subpixel accumulation doesn't propagate across embedded view.
            current.directly_composited_container_paint_offset_subpixel_delta =
                PhysicalOffset();
          }
          if (embedded_view->IsLocalFrameView()) {
            Walk(*To<LocalFrameView>(embedded_view), context);
          } else if (embedded_view->IsPluginView()) {
            // If it is a webview plugin, walk into the content frame view.
            if (auto* plugin_content_frame_view =
                    FindWebViewPluginContentFrameView(
                        *layout_embedded_content)) {
              Walk(*plugin_content_frame_view, context);
            }
          } else {
            // We need to do nothing for RemoteFrameView. See crbug.com/579281.
          }
        }
      }
    }
  }
  if (!pre_paint_info || pre_paint_info->is_last_for_node)
    object.GetMutableForPainting().ClearPaintFlags();
}

}  // namespace blink
