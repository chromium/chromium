// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
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

  bool updates_executed = root_frame_view.ExecuteAllPendingUpdates();
  if (updates_executed) {
    needs_invalidate_chrome_client_and_intersection_ = true;
  }

#if DCHECK_IS_ON()
  if ((needed_tree_builder_context_update || updates_executed) &&
      VLOG_IS_ON(1)) {
    ShowAllPropertyTrees(root_frame_view);
  }
#endif

  // If the page has anything changed, we need to inform the chrome client
  // so that the client will initiate repaint of the contents if needed (e.g.
  // when this page is embedded as a non-composited content of another page).
  if (needs_invalidate_chrome_client_and_intersection_) {
    if (auto* client = root_frame_view.GetChromeClient()) {
      client->InvalidateContainer();
    }
    // If any change needs a more significant intersection update in a frame
    // view, we should have set the state on that frame view during the tree
    // walk or earlier.
    if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
      root_frame_view.SetIntersectionObservationState(
          LocalFrameView::kScrollAndVisibilityOnly);
    }
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

  // Ensure the cached previous layout block in CaretDisplayItemClient is
  // invalidated and cleared even if the layout block is display locked.
  frame_view.GetFrame().Selection().EnsureInvalidationOfPreviousLayoutBlock();

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
  NOTREACHED_IN_MIGRATION();
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
      !context.blocking_wheel_event_handler_changed &&
      !object.ShouldInvalidatePaintForHitTestOnly()) {
    return;
  }

  context.paint_invalidator_context.painting_layer->SetNeedsRepaint();
  // We record hit test data when the painting layer repaints. No need to
  // invalidate the display item client.
  if (!RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    ObjectPaintInvalidator(object).InvalidateDisplayItemClient(
        object, PaintInvalidationReason::kHitTest);
  }
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
  NOTREACHED_IN_MIGRATION() << "Unknown reason.";
}
#endif

PrePaintInfo PrePaintTreeWalk::CreatePrePaintInfo(
    const PhysicalFragmentLink& child,
    const PrePaintTreeWalkContext& context) {
  const auto* fragment = To<PhysicalBoxFragment>(child.fragment.Get());
  return PrePaintInfo(fragment, child.offset,
                      context.current_container.fragmentainer_idx,
                      fragment->IsFirstForNode(), !fragment->GetBreakToken(),
                      /* is_inside_fragment_child */ false,
                      context.current_container.IsInFragmentationContext());
}

FragmentData* PrePaintTreeWalk::GetOrCreateFragmentData(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& context,
    const PrePaintInfo& pre_paint_info) {
  // If |allow_update| is set, we're allowed to add, remove and modify
  // FragmentData objects. Otherwise they will be left alone.
  bool allow_update = context.NeedsTreeBuilderContext();

  FragmentDataList& fragment_list =
      object.GetMutableForPainting().FragmentList();
  FragmentData* fragment_data = &fragment_list;

  // BR elements never fragment. While there are parts of the code that depend
  // on the correct paint offset (GetBoundingClientRect(), etc.), we don't need
  // to set fragmentation info (nor create multiple FragmentData entries). BR
  // elements aren't necessarily marked for invalidation when laid out (which
  // means that allow_update won't be set when it should, and the code below
  // would get confused).
  if (object.IsBR())
    return fragment_data;

  // The need for paint properties is the same across all fragments, so if the
  // first FragmentData needs it, so do all the others.
  bool needs_paint_properties = fragment_data->PaintProperties();

  wtf_size_t fragment_data_idx = 0;
  if (pre_paint_info.is_first_for_node) {
    if (const auto* layout_box = DynamicTo<LayoutBox>(&object)) {
      if (layout_box->PhysicalFragmentCount() != fragment_list.size()) {
        object.GetMutableForPainting().FragmentCountChanged();
      }
    }
  } else {
    if (pre_paint_info.is_inside_fragment_child) {
      if (!object.HasInlineFragments() && !IsLinkHighlighted(object)) {
        // We don't need any additional fragments for culled inlines - unless
        // this is the highlighted link (in which case even culled inlines get
        // paint effects).
        return nullptr;
      }

      const auto& parent_fragment = *pre_paint_info.box_fragment;
      // Find the start container fragment for this inline element, so that we
      // can figure out how far we've got, compared to that.
      InlineCursor cursor(
          *To<LayoutBlockFlow>(parent_fragment.GetLayoutObject()));
      cursor.MoveToIncludingCulledInline(object);
      DCHECK_GE(BoxFragmentIndex(parent_fragment),
                cursor.ContainerFragmentIndex());
      wtf_size_t parent_fragment_idx = BoxFragmentIndex(parent_fragment);

      const auto& container =
          *To<LayoutBlockFlow>(parent_fragment.GetLayoutObject());
      if (container.MayBeNonContiguousIfc()) {
        // The number of FragmentData entries must agree with the number of
        // fragments with items. Unfortunately, text and non-atomic inlines may
        // be "non-contiguous". This is for instance the case if there's a float
        // that takes up the entire fragmentainer somewhere in the middle (or at
        // the beginning, or at the end). Another example is during printing, if
        // monolithic content overflows and takes up the entire next page,
        // leaving no space for any line boxes that would otherwise be there.
        wtf_size_t walker_idx = cursor.ContainerFragmentIndex();
        bool found_in_parent = false;
        while (cursor.Current()) {
          cursor.MoveToNextForSameLayoutObject();
          wtf_size_t idx = cursor.ContainerFragmentIndex();
          if (walker_idx < idx) {
            // We've moved to the next fragmentainer where the object occurs.
            // Note that |idx| may have skipped fragmentainers here, if the
            // object isn't represented in some fragmentainer.
            if (idx > parent_fragment_idx) {
              // We've walked past the parent fragment.
              break;
            }
            fragment_data_idx++;
            walker_idx = idx;
          }
          if (idx == parent_fragment_idx) {
            found_in_parent = true;
            break;
          }
        }

        if (!found_in_parent) {
          return nullptr;
        }
      } else {
        // The inline formatting context is contiguous.
        fragment_data_idx =
            parent_fragment_idx - cursor.ContainerFragmentIndex();
      }
    } else {
      // Box fragments are always contiguous, i.e. fragmentainers are never
      // skipped.
      fragment_data_idx = BoxFragmentIndex(*pre_paint_info.box_fragment);
    }

    if (fragment_data_idx < fragment_list.size()) {
      fragment_data = &fragment_list.at(fragment_data_idx);
    } else {
      DCHECK(allow_update);
      fragment_data = &fragment_list.AppendNewFragment();
      DCHECK_EQ(fragment_data_idx + 1, fragment_list.size());

      // When we add FragmentData entries, we need to make sure that we update
      // paint properties. The object may not have been marked for an update, if
      // the reason for creating an additional FragmentData was that the
      // fragmentainer block-size shrunk, for instance.
      object.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
    }
  }

  if (pre_paint_info.is_last_for_node) {
    // We have reached the end. There may be more data entries that were
    // needed in the previous layout, but not any more. Clear them.
    if (allow_update) {
      fragment_list.Shrink(fragment_data_idx + 1);
    } else {
      DCHECK_EQ(fragment_data_idx + 1, fragment_list.size());
    }
  }

  if (allow_update) {
    fragment_data->SetFragmentID(pre_paint_info.fragmentainer_idx);
    if (needs_paint_properties)
      fragment_data->EnsurePaintProperties();
  } else {
    DCHECK_EQ(fragment_data->FragmentID(), pre_paint_info.fragmentainer_idx);
    DCHECK(!needs_paint_properties || fragment_data->PaintProperties());
  }

  return fragment_data;
}

void PrePaintTreeWalk::UpdateContextForOOFContainer(
    const LayoutObject& object,
    PrePaintTreeWalkContext& context,
    const PhysicalBoxFragment* fragment) {
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
                                    PrePaintInfo* pre_paint_info) {
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

  std::optional<PaintPropertyTreeBuilder> property_tree_builder;
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
          paint_invalidator_context)) {
    needs_invalidate_chrome_client_and_intersection_ = true;
  }

  InvalidatePaintForHitTesting(object, context);

  if (context.tree_builder_context) {
    property_tree_builder->UpdateForChildren();
    property_tree_builder->IssueInvalidationsAfterUpdate();
    needs_invalidate_chrome_client_and_intersection_ |=
        property_tree_builder->PropertiesChanged();
  }
}

bool PrePaintTreeWalk::CollectMissableChildren(
    PrePaintTreeWalkContext& context,
    const PhysicalBoxFragment& parent) {
  bool has_missable_children = false;
  for (const PhysicalFragmentLink& child : parent.Children()) {
    if (child->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }
    if (child->IsOutOfFlowPositioned() &&
        (context.current_container.fragment || child->IsFixedPositioned())) {
      // Add all out-of-flow positioned fragments inside a fragmentation
      // context. If a fragment is fixed-positioned, we even need to add those
      // that aren't inside a fragmentation context, because they may have an
      // ancestor LayoutObject inside one, and one of those ancestors may be
      // out-of-flow positioned, which may be missed, in which case we'll miss
      // this fixed-positioned one as well (since we don't enter descendant OOFs
      // when walking missed children) (example: fixedpos inside missed abspos
      // in relpos in multicol).
      pending_missables_.insert(child.fragment);
      has_missable_children = true;
    }
  }
  return has_missable_children;
}

const PhysicalBoxFragment* PrePaintTreeWalk::RebuildContextForMissedDescendant(
    const PhysicalBoxFragment& ancestor,
    const LayoutObject& object,
    bool update_tree_builder_context,
    PrePaintTreeWalkContext& context) {
  // Walk up to the ancestor and, on the way down again, adjust the context with
  // info about OOF containing blocks.
  if (&object == ancestor.OwnerLayoutBox()) {
    return &ancestor;
  }
  const PhysicalBoxFragment* search_fragment =
      RebuildContextForMissedDescendant(ancestor, *object.Parent(),
                                        update_tree_builder_context, context);

  if (object.IsLayoutFlowThread()) {
    // A flow threads doesn't create fragments. Just ignore it.
    return search_fragment;
  }

  const PhysicalBoxFragment* box_fragment = nullptr;
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
      for (PhysicalFragmentLink link : search_fragment->Children()) {
        if (link->GetLayoutObject() == object) {
          box_fragment = To<PhysicalBoxFragment>(link.get());
          paint_offset = link.offset;
          break;
        }
      }
    }

    // TODO(mstensho): Some of the bool parameters here are meaningless when
    // only used with PaintPropertyTreeBuilder (only used by
    // PrePaintTreeWalker). Consider cleaning this up, by splitting up
    // PrePaintInfo into one walker part and one builder part, so that we
    // don't have to specify them as false here.
    PrePaintInfo pre_paint_info(
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
      property_context.fragment_context;
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
    const PhysicalBoxFragment& fragment,
    bool is_in_fragment_traversal,
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

  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (child->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }
    if (!child->IsOutOfFlowPositioned()) {
      continue;
    }
    if (!pending_missables_.Contains(child.fragment))
      continue;
    const LayoutObject& descendant_object = *child->GetLayoutObject();
    PrePaintTreeWalkContext descendant_context(
        context, NeedsTreeBuilderContextUpdate(descendant_object, context));
    if (child->IsOutOfFlowPositioned()) {
      if (descendant_context.tree_builder_context.has_value()) {
        PaintPropertyTreeBuilderContext* builder_context =
            &descendant_context.tree_builder_context.value();
        builder_context->fragment_context.current.paint_offset +=
            offset_to_block_start_edge;
      }

      bool update_tree_builder_context =
          NeedsTreeBuilderContextUpdate(descendant_object, descendant_context);

      RebuildContextForMissedDescendant(fragment, *descendant_object.Parent(),
                                        update_tree_builder_context,
                                        descendant_context);
    }

    if (is_in_fragment_traversal) {
      PrePaintInfo pre_paint_info =
          CreatePrePaintInfo(child, descendant_context);
      Walk(descendant_object, descendant_context, &pre_paint_info);
    } else {
      Walk(descendant_object, descendant_context, /* pre_paint_info */ nullptr);
    }
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
    const PhysicalBoxFragment& fragment,
    const PrePaintTreeWalkContext& parent_context) {
  DCHECK(fragment.IsFragmentationContextRoot());

  if (fragment.IsPaginatedRoot()) {
    wtf_size_t fragmentainer_idx = 0;
    for (PhysicalFragmentLink child : fragment.Children()) {
      const auto* box_fragment = To<PhysicalBoxFragment>(child.fragment.Get());
      DCHECK_EQ(box_fragment->GetBoxType(), PhysicalFragment::kPageContainer);
      WalkPageContainer(child, object, parent_context, fragmentainer_idx);
      fragmentainer_idx++;
    }
    return;
  }

  std::optional<wtf_size_t> inner_fragmentainer_idx;

  for (PhysicalFragmentLink child : fragment.Children()) {
    const auto* box_fragment = To<PhysicalBoxFragment>(child.fragment.Get());
    if (box_fragment->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }

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

      PrePaintInfo pre_paint_info = CreatePrePaintInfo(child, parent_context);
      Walk(*box_fragment->GetLayoutObject(), parent_context, &pre_paint_info);
      continue;
    }

    // Check |box_fragment| and the |LayoutBox| that produced it are in sync.
    // |OwnerLayoutBox()| has a few DCHECKs for this purpose.
    DCHECK(box_fragment->OwnerLayoutBox());

    // Set up |inner_fragmentainer_idx| lazily, as it's O(n) (n == number of
    // multicol container fragments).
    if (!inner_fragmentainer_idx)
      inner_fragmentainer_idx = PreviousInnerFragmentainerIndex(fragment);

    WalkFragmentainer(object, child, parent_context, *inner_fragmentainer_idx);

    (*inner_fragmentainer_idx)++;
  }

  if (!To<LayoutBlockFlow>(&object)->MultiColumnFlowThread()) {
    return;
  }
  // Multicol containers only contain special legacy children invisible to
  // LayoutNG, so we need to clean them manually.
  if (fragment.GetBreakToken()) {
    return;  // Wait until we've reached the end.
  }
  for (const LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    DCHECK(child->IsLayoutFlowThread() || child->IsLayoutMultiColumnSet() ||
           child->IsLayoutMultiColumnSpannerPlaceholder());
    child->GetMutableForPainting().ClearPaintFlags();
  }
}

void PrePaintTreeWalk::WalkPageContainer(
    const PhysicalFragmentLink& page_container_link,
    const LayoutObject& parent_object,
    const PrePaintTreeWalkContext& parent_context,
    wtf_size_t fragmentainer_idx) {
  // In paginated layout, each fragmentainer (page area) is wrapped inside a
  // page box and a page border box.
  DCHECK_EQ(page_container_link->GetBoxType(),
            PhysicalFragment::kPageContainer);
  const auto& page_container =
      To<PhysicalBoxFragment>(*page_container_link.get());

  PrePaintTreeWalkContext page_container_context(
      parent_context, parent_context.NeedsTreeBuilderContext());
  PrePaintInfo container_pre_paint_info =
      CreatePrePaintInfo(page_container_link, page_container_context);
  WalkInternal(*page_container_link->GetLayoutObject(), page_container_context,
               &container_pre_paint_info);

  // Calculate the offset into the stitched coordinate system, where each page
  // is stacked after oneanother in the block direction. Example: in
  // horizontal-tb mode, if the page height is 800px and this is the third
  // page, the offset will 1600px.
  PhysicalOffset pagination_adjustment =
      StitchedPageContentRect(page_container).offset;

  for (const PhysicalFragmentLink& grandchild : page_container.Children()) {
    if (grandchild->GetBoxType() == PhysicalFragment::kPageMargin) {
      // This is one of 16 possible page margin boxes, e.g. used to display page
      // headers or footers.
      PrePaintTreeWalkContext margin_box_context(
          parent_context, parent_context.NeedsTreeBuilderContext());
      PrePaintInfo margin_pre_paint_info =
          CreatePrePaintInfo(grandchild, margin_box_context);
      Walk(*grandchild->GetLayoutObject(), margin_box_context,
           &margin_pre_paint_info);
      continue;
    }

    DCHECK_EQ(grandchild->GetBoxType(), PhysicalFragment::kPageBorderBox);

    // This is a page border box, which contains the page contents area fragment
    // (the fragmentainer that contains a portion of the document's fragmented
    // contents).
    PrePaintTreeWalkContext page_border_box_context(
        page_container_context,
        page_container_context.NeedsTreeBuilderContext());
    if (page_border_box_context.tree_builder_context) {
      PrePaintInfo border_box_pre_paint_info =
          CreatePrePaintInfo(grandchild, page_border_box_context);
      PaintPropertyTreeBuilder builder(
          *grandchild->GetLayoutObject(), &border_box_pre_paint_info,
          page_border_box_context.tree_builder_context.value());
      builder.UpdateForPageBorderBox(page_container);
    }

    // A page border box fragment should only have one child: the page area.
    const PhysicalFragmentLink& page_area = grandchild->Children()[0];
    DCHECK_EQ(page_area->GetBoxType(), PhysicalFragment::kPageArea);

    PrePaintTreeWalkContext page_area_context(
        parent_context, parent_context.NeedsTreeBuilderContext());
    PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext*
        containing_block_context = nullptr;

    if (page_area_context.tree_builder_context) {
      PaintPropertyTreeBuilderFragmentContext& fragment_context =
          page_area_context.tree_builder_context->fragment_context;
      containing_block_context = &fragment_context.current;
      containing_block_context->paint_offset += pagination_adjustment;

      PaginationState* pagination_state =
          parent_object.GetFrameView()->GetPaginationState();
      ObjectPaintProperties& pagination_paint_properties =
          pagination_state->EnsureContentAreaProperties(
              *containing_block_context->transform,
              *containing_block_context->clip);
      // Insert transform and clipping nodes between the paint properties of the
      // LayoutView and the document contents. They will be updated as each page
      // is painted.
      containing_block_context->transform =
          pagination_paint_properties.Transform();
      containing_block_context->clip =
          pagination_paint_properties.OverflowClip();
    }

    WalkFragmentainer(parent_object, page_area, page_area_context,
                      fragmentainer_idx);

    if (containing_block_context) {
      containing_block_context->paint_offset -= pagination_adjustment;
    }
  }
}

void PrePaintTreeWalk::WalkFragmentainer(
    const LayoutObject& parent_object,
    const PhysicalFragmentLink& child_link,
    const PrePaintTreeWalkContext& parent_context,
    wtf_size_t fragmentainer_idx) {
  DCHECK(child_link->IsFragmentainerBox());
  const auto& fragmentainer = To<PhysicalBoxFragment>(*child_link.get());

  PrePaintTreeWalkContext fragmentainer_context(
      parent_context, parent_context.NeedsTreeBuilderContext());

  fragmentainer_context.current_container.fragmentation_nesting_level++;
  fragmentainer_context.is_parent_first_for_node =
      fragmentainer.IsFirstForNode();

  // Always keep track of the current innermost fragmentainer we're handling, as
  // they may serve as containing blocks for OOF descendants.
  fragmentainer_context.current_container.fragment = &fragmentainer;

  fragmentainer_context.current_container.fragmentainer_idx = fragmentainer_idx;

  PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext*
      containing_block_context = nullptr;
  if (fragmentainer_context.tree_builder_context) {
    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        fragmentainer_context.tree_builder_context->fragment_context;
    containing_block_context = &fragment_context.current;
    containing_block_context->paint_offset += child_link.offset;

    // Keep track of the paint offset at the fragmentainer. This is needed when
    // entering OOF descendants. OOFs have the nearest fragmentainer as their
    // containing block, so when entering them during LayoutObject tree
    // traversal, we have to compensate for this.
    containing_block_context->paint_offset_for_oof_in_fragmentainer =
        containing_block_context->paint_offset;

    if (parent_object.IsLayoutView()) {
      // Out-of-flow positioned descendants are positioned relatively to this
      // fragmentainer (page).
      fragment_context.absolute_position = *containing_block_context;
      fragment_context.fixed_position = *containing_block_context;
    }
  }

  // If this is a multicol container, the actual children are inside the flow
  // thread child of |parent_object|.
  const auto* flow_thread =
      To<LayoutBlockFlow>(&parent_object)->MultiColumnFlowThread();
  const auto& actual_parent = flow_thread ? *flow_thread : parent_object;
  WalkChildren(actual_parent, &fragmentainer, fragmentainer_context);

  if (containing_block_context) {
    containing_block_context->paint_offset -= child_link.offset;
  }
}

void PrePaintTreeWalk::WalkLayoutObjectChildren(
    const LayoutObject& parent_object,
    const PhysicalBoxFragment* parent_fragment,
    const PrePaintTreeWalkContext& context) {
  std::optional<InlineCursor> inline_cursor;
  for (const LayoutObject* child = parent_object.SlowFirstChild(); child;
       // Stay on the |child| while iterating fragments of |child|.
       child = inline_cursor ? child : child->NextSibling()) {
    if (!parent_fragment) {
      // If we haven't found a fragment tree to accompany us in our walk,
      // perform a pure LayoutObject tree walk. This is needed for legacy block
      // fragmentation, and it also works fine if there's no block fragmentation
      // involved at all (in such cases we can either to do this, or perform the
      // PhysicalBoxFragment-accompanied walk that we do further down).

      if (child->IsLayoutMultiColumnSpannerPlaceholder()) {
        child->GetMutableForPainting().ClearPaintFlags();
        continue;
      }

      Walk(*child, context, /* pre_paint_info */ nullptr);
      continue;
    }

    // Perform an PhysicalBoxFragment-accompanied walk of the child
    // LayoutObject tree.
    //
    // We'll map each child LayoutObject to a corresponding
    // PhysicalBoxFragment. For text and non-atomic inlines this will be the
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
    const PhysicalBoxFragment* box_fragment = nullptr;
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
        const FragmentItem& item = *inline_cursor->Current().Item();
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
          is_last_for_node = !box_fragment->GetBreakToken();
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
      // a block before we got to any inline content).

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

        bool child_has_any_items;
        if (!parent_fragment->Items()->IsContainerForCulledInline(
                *layout_inline_child, &is_first_for_node, &is_last_for_node,
                &child_has_any_items)) {
          if (child_has_any_items) {
            continue;
          }
          // The inline has no fragment items inside, although it does have
          // child objects. This may happen for an AREA elements with
          // out-of-flow positioned children.
          //
          // Set the first/last flags, since they may have been messed up above.
          // This means that we're going to visit the descendants for every
          // container fragment that has items, but this harmless, and rare.
          is_first_for_node = true;
          is_last_for_node = true;
        }
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
      const PhysicalBoxFragment* search_fragment = parent_fragment;
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
        for (PhysicalFragmentLink link : search_fragment->Children()) {
          if (link->GetLayoutObject() != child)
            continue;
          // We found it!
          box_fragment = To<PhysicalBoxFragment>(link.get());
          paint_offset = link.offset;
          is_first_for_node = box_fragment->IsFirstForNode();
          is_last_for_node = !box_fragment->GetBreakToken();
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
      PrePaintInfo pre_paint_info(
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
    const PhysicalBoxFragment* traversable_fragment,
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
          To<PhysicalBoxFragment>(box->GetPhysicalFragment(0));
      DCHECK(!first_fragment->GetBreakToken());
      if (first_fragment->IsFragmentationContextRoot() &&
          box->CanTraversePhysicalFragments())
        traversable_fragment = first_fragment;
    }
  }

  // Keep track of fragments that act as containers for OOFs, so that we can
  // search their children when looking for an OOF further down in the tree.
  UpdateContextForOOFContainer(object, context, traversable_fragment);

  bool has_missable_children = false;
  const PhysicalBoxFragment* fragment = traversable_fragment;
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
    WalkMissedChildren(*fragment, !!traversable_fragment, context);
  }
}

void PrePaintTreeWalk::Walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parent_context,
                            PrePaintInfo* pre_paint_info) {
  const PhysicalBoxFragment* physical_fragment = nullptr;
  bool is_inside_fragment_child = false;
  if (pre_paint_info) {
    physical_fragment = pre_paint_info->box_fragment;
    DCHECK(physical_fragment);
    is_inside_fragment_child = pre_paint_info->is_inside_fragment_child;
  }

  // If we're visiting a missable fragment, remove it from the list.
  if (object.IsOutOfFlowPositioned()) {
    if (physical_fragment) {
      pending_missables_.erase(physical_fragment);
    } else {
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
            auto& current =
                context.tree_builder_context->fragment_context.current;
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
