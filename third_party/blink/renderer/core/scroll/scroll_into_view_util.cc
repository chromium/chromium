// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"

#include <optional>
#include <tuple>

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

// Returns true if a scroll into view can continue to cause scrolling in the
// parent frame.
bool AllowedToPropagateToParent(
    const LocalFrame& from_frame,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  // Focused editable scrolling (i.e. scroll an input the user tapped on)
  // always originates from a user action in the browser so it should always be
  // allowed to cross origins and we shouldn't stop it for policy or other
  // reasons.
  DCHECK(!params->for_focused_editable || params->cross_origin_boundaries);
  if (params->for_focused_editable)
    return true;

  // TODO(bokan): For now, we'll do the safe thing and just block all other
  // types of scrollIntoView from propagating out of a fenced frame but we may
  // need to loosen this if we find other critical use cases.
  // https://crbug.com/1324816.
  if (from_frame.IsFencedFrameRoot())
    return false;

  if (!params->cross_origin_boundaries) {
    Frame* parent_frame = from_frame.Tree().Parent();
    if (parent_frame &&
        !parent_frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            from_frame.GetSecurityContext()->GetSecurityOrigin())) {
      return false;
    }
  }

  if (params->type != mojom::blink::ScrollType::kProgrammatic)
    return true;

  if (!from_frame.GetDocument())
    return true;

  return !from_frame.GetDocument()->IsVerticalScrollEnforced();
}

ALWAYS_INLINE ScrollableArea* GetScrollableAreaForLayoutBox(
    const LayoutBox& box,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  if (box.IsScrollContainer() && !box.IsLayoutView()) {
    return box.GetScrollableArea();
  } else if (!box.ContainingBlock()) {
    return params->make_visible_in_visual_viewport
               ? box.GetFrameView()->GetScrollableArea()
               : box.GetFrameView()->LayoutViewport();
  }
  return nullptr;
}

// Helper to return the parent LayoutBox, crossing local frame boundaries, that
// a scroll should bubble up to or nullptr if the local root has been reached.
// The return optional will be empty if the scroll is blocked from bubbling to
// the root.
std::optional<LayoutBox*> GetScrollParent(
    const LayoutBox& box,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  bool is_fixed_to_frame = box.StyleRef().GetPosition() == EPosition::kFixed &&
                           box.Container() == box.View();

  // Within a document scrolls bubble along the containing block chain but if
  // we're in a position:fixed element, we want to immediately bubble up across
  // the frame boundary since scrolling the frame won't affect the box's
  // position.
  if (box.ContainingBlock() && !is_fixed_to_frame)
    return box.ContainingBlock();

  // Otherwise, we're bubbling across a frame boundary. We may be
  // prevented from doing so for security or policy reasons. If so, we're
  // done.
  if (!AllowedToPropagateToParent(*box.GetFrame(), params))
    return std::nullopt;

  if (!box.GetFrame()->IsLocalRoot()) {
    // The parent is a local iframe, convert to the absolute coordinate space
    // of its document and continue from the owner's LayoutBox.
    HTMLFrameOwnerElement* owner_element = box.GetDocument().LocalOwner();
    DCHECK(owner_element);

    // A display:none iframe can have a LayoutView but its owner element won't
    // have a LayoutObject. If that happens, don't bubble the scroll.
    if (!owner_element->GetLayoutObject())
      return std::nullopt;

    return owner_element->GetLayoutObject()->EnclosingBox();
  }

  // If the owner is remote, the scroll must continue via IPC.
  DCHECK(box.GetFrame()->IsMainFrame() ||
         box.GetFrame()->Parent()->IsRemoteFrame());
  return nullptr;
}

ALWAYS_INLINE void AdjustRectToNotEmpty(PhysicalRect& rect) {
  if (rect.Width() <= 0) {
    rect.SetWidth(LayoutUnit(1));
  }
  if (rect.Height() <= 0) {
    rect.SetHeight(LayoutUnit(1));
  }
}

ALWAYS_INLINE void AdjustRectAndParamsForParentFrame(
    const LayoutBox& current_box,
    const LayoutBox* next_box,
    PhysicalRect& absolute_rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr& params) {
  // If the next box to scroll is in another frame, we need to convert the
  // scroll box to the new frame's absolute coordinates.
  if (next_box && next_box->View() != current_box.View()) {
    scroll_into_view_util::ConvertParamsToParentFrame(
        params, gfx::RectF(absolute_rect_to_scroll), *current_box.View(),
        *next_box->View());

    absolute_rect_to_scroll = current_box.View()->LocalToAncestorRect(
        absolute_rect_to_scroll, next_box->View(), kTraverseDocumentBoundaries);
  }
}

// Helper that reveals the given rect, given in absolute coordinates, by
// scrolling the given `box` LayoutBox and then all its ancestors up to the
// local root frame.  To continue the reveal through remote ancestors, use
// LayoutObject::ScrollRectToVisible. If the scroll bubbled up to the local
// root successfully, returns the updated absolute rect in the absolute
// coordinates of the local root. Otherwise returns an empty optional.
std::optional<PhysicalRect> PerformBubblingScrollIntoView(
    const LayoutBox& box,
    const PhysicalRect& absolute_rect,
    mojom::blink::ScrollIntoViewParamsPtr& params,
    const PhysicalBoxStrut& scroll_margin,
    bool from_remote_frame) {
  DCHECK(params->type == mojom::blink::ScrollType::kProgrammatic ||
         params->type == mojom::blink::ScrollType::kUser);

  if (!box.GetFrameView())
    return std::nullopt;

  PhysicalRect absolute_rect_to_scroll = absolute_rect;
  PhysicalBoxStrut active_scroll_margin = scroll_margin;
  bool scrolled_to_area = false;
  bool will_sequence_scrolls =
      !RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled() &&
      params->is_for_scroll_sequence;

  // TODO(bokan): Temporary, to track cross-origin scroll-into-view prevalence.
  // https://crbug.com/1339003.
  const SecurityOrigin* starting_frame_origin =
      box.GetFrame()->GetSecurityContext()->GetSecurityOrigin();

  const LayoutBox* current_box = &box;
  while (current_box) {
    AdjustRectToNotEmpty(absolute_rect_to_scroll);

    // If we've reached the main frame's layout viewport (which is always set to
    // the global root scroller, see ViewportScrollCallback::SetScroller), if
    // this scroll-into-view is for focusing an editable. We do this so
    // that we can allow a smooth "scroll and zoom" animation to do the final
    // scroll in cases like scrolling a focused editable box into view.
    // TODO(bokan): Ensure a fenced frame doesn't get a global root scroller
    // and then remove the !IsInFencedFrameTree condition.
    // https://crbug.com/1314858
    if (!current_box->GetFrame()->IsInFencedFrameTree() &&
        params->for_focused_editable && current_box->IsGlobalRootScroller()) {
      break;
    }

    ScrollableArea* area_to_scroll =
        GetScrollableAreaForLayoutBox(*current_box, params);
    if (area_to_scroll) {
      ScrollOffset scroll_before = area_to_scroll->GetScrollOffset();
      CHECK(!will_sequence_scrolls ||
            area_to_scroll->GetSmoothScrollSequencer());
      wtf_size_t num_scroll_sequences =
          will_sequence_scrolls
              ? area_to_scroll->GetSmoothScrollSequencer()->GetCount()
              : 0ul;

      absolute_rect_to_scroll = area_to_scroll->ScrollIntoView(
          absolute_rect_to_scroll, active_scroll_margin, params);
      scrolled_to_area = true;

      // TODO(bokan): Temporary, to track cross-origin scroll-into-view
      // prevalence. https://crbug.com/1339003.
      // If this is for a scroll sequence, GetScrollOffset won't change until
      // all the animations in the sequence are run which happens
      // asynchronously after this method returns. Thus, for scroll sequences,
      // check instead if an entry was added to the sequence which occurs only
      // if the scroll offset is changed as a result of ScrollIntoView.
      bool scroll_changed =
          will_sequence_scrolls
              ? area_to_scroll->GetSmoothScrollSequencer()->GetCount() !=
                    num_scroll_sequences
              : area_to_scroll->GetScrollOffset() != scroll_before;
      if (scroll_changed && !params->for_focused_editable &&
          params->type == mojom::blink::ScrollType::kProgrammatic) {
        const SecurityOrigin* current_frame_origin =
            current_box->GetFrame()->GetSecurityContext()->GetSecurityOrigin();
        if (!current_frame_origin->CanAccess(starting_frame_origin) ||
            from_remote_frame) {
          // ScrollIntoView caused a visible scroll in an origin that can't be
          // accessed from where the ScrollIntoView was initiated.
          DCHECK(params->cross_origin_boundaries);
          UseCounter::Count(
              current_box->GetFrame()->LocalFrameRoot().GetDocument(),
              WebFeature::kCrossOriginScrollIntoView);
        }
      }
    }

    bool is_fixed_to_frame =
        current_box->StyleRef().GetPosition() == EPosition::kFixed &&
        current_box->Container() == current_box->View();

    VisualViewport& visual_viewport =
        current_box->GetFrame()->GetPage()->GetVisualViewport();
    if (is_fixed_to_frame && params->make_visible_in_visual_viewport) {
      // If we're in a position:fixed element, scrolling the layout viewport
      // won't have any effect and would be wrong so we want to bubble up to
      // the layout viewport's parent. For subframes that's the frame's owner.
      // For the main frame that's the visual viewport but it isn't associated
      // with a LayoutBox so we just scroll it here as a special case.
      // Note: In non-fixed cases, the visual viewport will have been scrolled
      // by the frame scroll via the RootFrameViewport
      // (GetFrameView()->GetScrollableArea() above).
      if (current_box->GetFrame()->IsMainFrame() &&
          visual_viewport.IsActiveViewport()) {
        absolute_rect_to_scroll =
            current_box->GetFrame()
                ->GetPage()
                ->GetVisualViewport()
                .ScrollIntoView(absolute_rect_to_scroll, active_scroll_margin,
                                params);
        scrolled_to_area = true;
      }

      // TODO(bokan): To be correct we should continue to bubble the scroll
      // from a subframe since ancestor frames can still scroll the element
      // into view. However, making that change had some compat-impact so we
      // intentionally keep this behavior for now while
      // https://crbug.com/1334265 is resolved.
      break;
    }

    // If the scroll was stopped prior to reaching the local root, we cannot
    // return a rect since the caller cannot know which frame it's relative to.
    std::optional<LayoutBox*> next_box_opt =
        GetScrollParent(*current_box, params);
    if (!next_box_opt) {
      return std::nullopt;
    }

    LayoutBox* next_box = *next_box_opt;

    AdjustRectAndParamsForParentFrame(*current_box, next_box,
                                      absolute_rect_to_scroll, params);

    // Once we've taken the scroll-margin into account, don't apply it to
    // ancestor scrollers.
    // TODO(crbug.com/1325839): Instead of just nullifying the scroll-margin,
    // maybe we should be applying the scroll-margin of the containing
    // scrollers themselves? This will probably need to be spec'd as the current
    // scroll-into-view spec[1] only refers to the bounding border box.
    // [1] https://drafts.csswg.org/cssom-view-1/#scroll-a-target-into-view
    if (scrolled_to_area) {
      active_scroll_margin = PhysicalBoxStrut();
    }

    current_box = next_box;
  }

  return absolute_rect_to_scroll;
}

}  // namespace

namespace scroll_into_view_util {

void ScrollRectToVisible(const LayoutObject& layout_object,
                         const PhysicalRect& absolute_rect,
                         mojom::blink::ScrollIntoViewParamsPtr params,
                         bool from_remote_frame) {
  LayoutBox* enclosing_box = layout_object.EnclosingBox();
  if (!enclosing_box)
    return;

  LocalFrame* frame = layout_object.GetFrame();

  params->is_for_scroll_sequence |=
      params->type == mojom::blink::ScrollType::kProgrammatic;
  bool will_sequence_scrolls =
      !RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled() &&
      params->is_for_scroll_sequence;

  SmoothScrollSequencer* old_sequencer = nullptr;
  if (will_sequence_scrolls) {
    old_sequencer = frame->CreateNewSmoothScrollSequence();
    frame->GetSmoothScrollSequencer()->SetScrollType(params->type);
  }

  PhysicalBoxStrut scroll_margin =
      layout_object.Style() ? layout_object.Style()->ScrollMarginStrut()
                            : PhysicalBoxStrut();
  PhysicalRect absolute_rect_to_scroll = absolute_rect;
  absolute_rect_to_scroll.Expand(scroll_margin);
  std::optional<PhysicalRect> updated_absolute_rect =
      PerformBubblingScrollIntoView(*enclosing_box, absolute_rect_to_scroll,
                                    params, scroll_margin, from_remote_frame);

  if (will_sequence_scrolls) {
    if (frame->GetSmoothScrollSequencer()->IsEmpty()) {
      // If the scroll into view was a no-op (the element was already in the
      // proper place), reinstate any previously running smooth scroll sequence
      // so that it can continue running. This prevents unintentionally
      // clobbering a scroll by e.g. setting focus() to an in-view element.
      frame->ReinstateSmoothScrollSequence(old_sequencer);
    } else {
      // Otherwise clobber any previous sequence.
      if (old_sequencer) {
        old_sequencer->AbortAnimations();
      }
      frame->GetSmoothScrollSequencer()->RunQueuedAnimations();
    }
  }

  // If the scroll into view stopped early (i.e. before the local root),
  // there's no need to continue bubbling or finishing a scroll focused
  // editable into view.
  if (!updated_absolute_rect)
    return;

  LocalFrame& local_root = frame->LocalFrameRoot();
  LocalFrameView* local_root_view = local_root.View();

  if (!local_root_view)
    return;

  if (!local_root.IsOutermostMainFrame()) {
    // Continue the scroll via IPC if there's a remote ancestor.
    if (AllowedToPropagateToParent(local_root, params)) {
      local_root_view->ScrollRectToVisibleInRemoteParent(*updated_absolute_rect,
                                                         std::move(params));
    }
  } else if (params->for_focused_editable) {
    // If we're scrolling a focused editable into view, once we reach the main
    // frame we need to perform an animated scroll and zoom to bring the
    // editable into a legible size.
    gfx::RectF caret_rect_in_root_frame(*updated_absolute_rect);
    DCHECK(!caret_rect_in_root_frame.IsEmpty());
    local_root.GetPage()->GetChromeClient().FinishScrollFocusedEditableIntoView(
        caret_rect_in_root_frame, std::move(params));
  }
}

gfx::RectF FocusedEditableBoundsFromParams(
    const gfx::RectF& caret_rect,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  DCHECK(params->for_focused_editable);
  DCHECK(!params->for_focused_editable->size.IsEmpty());

  gfx::PointF editable_location =
      caret_rect.origin() + params->for_focused_editable->relative_location;
  return gfx::RectF(editable_location, params->for_focused_editable->size);
}

void ConvertParamsToParentFrame(mojom::blink::ScrollIntoViewParamsPtr& params,
                                const gfx::RectF& caret_rect_in_src,
                                const LayoutObject& src_frame,
                                const LayoutView& dest_frame) {
  if (!params->for_focused_editable)
    return;

  // The source frame will be a LayoutView if the conversion is local or a
  // LayoutEmbeddedContent if we're crossing a remote boundary.
  DCHECK(src_frame.IsLayoutView() || src_frame.IsLayoutEmbeddedContent());

  gfx::RectF editable_bounds_in_src =
      FocusedEditableBoundsFromParams(caret_rect_in_src, params);

  PhysicalRect editable_bounds_in_dest = src_frame.LocalToAncestorRect(
      PhysicalRect::EnclosingRect(editable_bounds_in_src), &dest_frame,
      kTraverseDocumentBoundaries);

  PhysicalRect caret_rect_in_dest = src_frame.LocalToAncestorRect(
      PhysicalRect::EnclosingRect(caret_rect_in_src), &dest_frame,
      kTraverseDocumentBoundaries);

  params->for_focused_editable->relative_location = gfx::Vector2dF(
      editable_bounds_in_dest.offset - caret_rect_in_dest.offset);
  params->for_focused_editable->size = gfx::SizeF(editable_bounds_in_dest.size);

  DCHECK(!params->for_focused_editable->size.IsEmpty());
}

mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    const mojom::blink::ScrollAlignment& align_x,
    const mojom::blink::ScrollAlignment& align_y,
    mojom::blink::ScrollType scroll_type,
    bool make_visible_in_visual_viewport,
    mojom::blink::ScrollBehavior scroll_behavior,
    bool is_for_scroll_sequence,
    bool cross_origin_boundaries) {
  auto params = mojom::blink::ScrollIntoViewParams::New();
  params->align_x = mojom::blink::ScrollAlignment::New(align_x);
  params->align_y = mojom::blink::ScrollAlignment::New(align_y);
  params->type = scroll_type;
  params->make_visible_in_visual_viewport = make_visible_in_visual_viewport;
  params->behavior = scroll_behavior;
  params->is_for_scroll_sequence = is_for_scroll_sequence;
  params->cross_origin_boundaries = cross_origin_boundaries;
  return params;
}

namespace {
mojom::blink::ScrollAlignment ResolveToPhysicalAlignment(
    V8ScrollLogicalPosition::Enum inline_alignment,
    V8ScrollLogicalPosition::Enum block_alignment,
    ScrollOrientation axis,
    const ComputedStyle& computed_style) {
  bool is_horizontal_writing_mode = computed_style.IsHorizontalWritingMode();
  V8ScrollLogicalPosition::Enum alignment =
      ((axis == kHorizontalScroll && is_horizontal_writing_mode) ||
       (axis == kVerticalScroll && !is_horizontal_writing_mode))
          ? inline_alignment
          : block_alignment;

  if (alignment == V8ScrollLogicalPosition::Enum::kCenter) {
    return ScrollAlignment::CenterAlways();
  }
  if (alignment == V8ScrollLogicalPosition::Enum::kNearest) {
    return ScrollAlignment::ToEdgeIfNeeded();
  }
  if (alignment == V8ScrollLogicalPosition::Enum::kStart) {
    PhysicalToLogical<const mojom::blink::ScrollAlignment& (*)()> to_logical(
        computed_style.GetWritingDirection(), ScrollAlignment::TopAlways,
        ScrollAlignment::RightAlways, ScrollAlignment::BottomAlways,
        ScrollAlignment::LeftAlways);
    if (axis == kHorizontalScroll) {
      return is_horizontal_writing_mode ? (*to_logical.InlineStart())()
                                        : (*to_logical.BlockStart())();
    } else {
      return is_horizontal_writing_mode ? (*to_logical.BlockStart())()
                                        : (*to_logical.InlineStart())();
    }
  }
  if (alignment == V8ScrollLogicalPosition::Enum::kEnd) {
    PhysicalToLogical<const mojom::blink::ScrollAlignment& (*)()> to_logical(
        computed_style.GetWritingDirection(), ScrollAlignment::TopAlways,
        ScrollAlignment::RightAlways, ScrollAlignment::BottomAlways,
        ScrollAlignment::LeftAlways);
    if (axis == kHorizontalScroll) {
      return is_horizontal_writing_mode ? (*to_logical.InlineEnd())()
                                        : (*to_logical.BlockEnd())();
    } else {
      return is_horizontal_writing_mode ? (*to_logical.BlockEnd())()
                                        : (*to_logical.InlineEnd())();
    }
  }

  // Default values
  if (is_horizontal_writing_mode) {
    return (axis == kHorizontalScroll) ? ScrollAlignment::ToEdgeIfNeeded()
                                       : ScrollAlignment::TopAlways();
  }
  return (axis == kHorizontalScroll) ? ScrollAlignment::LeftAlways()
                                     : ScrollAlignment::ToEdgeIfNeeded();
}

V8ScrollLogicalPosition::Enum SnapAlignmentToV8ScrollLogicalPosition(
    cc::SnapAlignment alignment) {
  switch (alignment) {
    case cc::SnapAlignment::kNone:
      return V8ScrollLogicalPosition::Enum::kNearest;
    case cc::SnapAlignment::kStart:
      return V8ScrollLogicalPosition::Enum::kStart;
    case cc::SnapAlignment::kEnd:
      return V8ScrollLogicalPosition::Enum::kEnd;
    case cc::SnapAlignment::kCenter:
      return V8ScrollLogicalPosition::Enum::kCenter;
  }
}

}  // namespace

mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    const ScrollIntoViewOptions& options,
    const ComputedStyle& computed_style) {
  mojom::blink::ScrollBehavior behavior = mojom::blink::ScrollBehavior::kAuto;
  if (options.behavior().AsEnum() == V8ScrollBehavior::Enum::kSmooth) {
    behavior = mojom::blink::ScrollBehavior::kSmooth;
  }
  if (options.behavior() == V8ScrollBehavior::Enum::kInstant) {
    behavior = mojom::blink::ScrollBehavior::kInstant;
  }

  auto align_x = ResolveToPhysicalAlignment(options.inlinePosition().AsEnum(),
                                            options.block().AsEnum(),
                                            kHorizontalScroll, computed_style);
  auto align_y = ResolveToPhysicalAlignment(options.inlinePosition().AsEnum(),
                                            options.block().AsEnum(),
                                            kVerticalScroll, computed_style);

  mojom::blink::ScrollIntoViewParamsPtr params =
      CreateScrollIntoViewParams(align_x, align_y);
  params->behavior = behavior;
  return params;
}

mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    const ComputedStyle& computed_style) {
  V8ScrollLogicalPosition::Enum inline_alignment =
      SnapAlignmentToV8ScrollLogicalPosition(
          computed_style.GetScrollSnapAlign().alignment_inline);
  V8ScrollLogicalPosition::Enum block_alignment =
      SnapAlignmentToV8ScrollLogicalPosition(
          computed_style.GetScrollSnapAlign().alignment_block);
  auto align_x = ResolveToPhysicalAlignment(inline_alignment, block_alignment,
                                            kHorizontalScroll, computed_style);
  auto align_y = ResolveToPhysicalAlignment(inline_alignment, block_alignment,
                                            kVerticalScroll, computed_style);

  mojom::blink::ScrollIntoViewParamsPtr params =
      CreateScrollIntoViewParams(align_x, align_y);
  params->behavior = computed_style.GetScrollBehavior();
  return params;
}

ScrollOffset GetScrollOffsetToExpose(
    const ScrollableArea& scroll_area,
    const PhysicalRect& local_expose_rect,
    const PhysicalBoxStrut& expose_scroll_margin,
    const mojom::blink::ScrollAlignment& align_x,
    const mojom::blink::ScrollAlignment& align_y) {
  // Represent the rect in the container's scroll-origin coordinate.
  PhysicalRect scroll_origin_to_expose_rect = local_expose_rect;
  scroll_origin_to_expose_rect.Move(scroll_area.LocalToScrollOriginOffset());
  // Prevent degenerate cases by giving the visible rect a minimum non-0 size.
  PhysicalRect non_zero_visible_rect = scroll_area.VisibleScrollSnapportRect();
  ScrollOffset current_scroll_offset = scroll_area.GetScrollOffset();
  LayoutUnit minimum_layout_unit;
  minimum_layout_unit.SetRawValue(1);
  if (non_zero_visible_rect.Width() <= LayoutUnit()) {
    non_zero_visible_rect.SetWidth(minimum_layout_unit);
  }
  if (non_zero_visible_rect.Height() <= LayoutUnit()) {
    non_zero_visible_rect.SetHeight(minimum_layout_unit);
  }

  // The scroll_origin_to_expose_rect includes the scroll-margin of the element
  // that is being exposed. We want to exclude the margin for deciding whether
  // it's already visible, but include it when calculating the scroll offset
  // that we need to scroll to in order to achieve the desired alignment.
  PhysicalRect expose_rect_no_margin = scroll_origin_to_expose_rect;
  expose_rect_no_margin.Contract(expose_scroll_margin);

  // Determine the appropriate X behavior.
  mojom::blink::ScrollAlignment::Behavior scroll_x;
  PhysicalRect expose_rect_x(
      expose_rect_no_margin.X(), non_zero_visible_rect.Y(),
      expose_rect_no_margin.Width(), non_zero_visible_rect.Height());
  LayoutUnit intersect_width =
      Intersection(non_zero_visible_rect, expose_rect_x).Width();
  if (intersect_width == expose_rect_no_margin.Width()) {
    // If the rectangle is fully visible, use the specified visible behavior.
    // If the rectangle is partially visible, but over a certain threshold,
    // then treat it as fully visible to avoid unnecessary horizontal scrolling
    scroll_x = align_x.rect_visible;
  } else if (intersect_width == non_zero_visible_rect.Width()) {
    // The rect is bigger than the visible area.
    scroll_x = align_x.rect_visible;
  } else if (intersect_width > 0) {
    // If the rectangle is partially visible, but not above the minimum
    // threshold, use the specified partial behavior
    scroll_x = align_x.rect_partial;
  } else {
    scroll_x = align_x.rect_hidden;
  }

  if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kClosestEdge) {
    // Closest edge is the right in two cases:
    // (1) exposeRect to the right of and smaller than nonZeroVisibleRect
    // (2) exposeRect to the left of and larger than nonZeroVisibleRect
    if ((scroll_origin_to_expose_rect.Right() > non_zero_visible_rect.Right() &&
         scroll_origin_to_expose_rect.Width() <
             non_zero_visible_rect.Width()) ||
        (scroll_origin_to_expose_rect.Right() < non_zero_visible_rect.Right() &&
         scroll_origin_to_expose_rect.Width() >
             non_zero_visible_rect.Width())) {
      scroll_x = mojom::blink::ScrollAlignment::Behavior::kRight;
    }
  }

  // Determine the appropriate Y behavior.
  mojom::blink::ScrollAlignment::Behavior scroll_y;
  PhysicalRect expose_rect_y(
      non_zero_visible_rect.X(), expose_rect_no_margin.Y(),
      non_zero_visible_rect.Width(), expose_rect_no_margin.Height());
  LayoutUnit intersect_height =
      Intersection(non_zero_visible_rect, expose_rect_y).Height();
  if (intersect_height == expose_rect_no_margin.Height()) {
    // If the rectangle is fully visible, use the specified visible behavior.
    scroll_y = align_y.rect_visible;
  } else if (intersect_height == non_zero_visible_rect.Height()) {
    // The rect is bigger than the visible area.
    scroll_y = align_y.rect_visible;
  } else if (intersect_height > 0) {
    // If the rectangle is partially visible, use the specified partial behavior
    scroll_y = align_y.rect_partial;
  } else {
    scroll_y = align_y.rect_hidden;
  }

  if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kClosestEdge) {
    // Closest edge is the bottom in two cases:
    // (1) exposeRect below and smaller than nonZeroVisibleRect
    // (2) exposeRect above and larger than nonZeroVisibleRect
    if ((scroll_origin_to_expose_rect.Bottom() >
             non_zero_visible_rect.Bottom() &&
         scroll_origin_to_expose_rect.Height() <
             non_zero_visible_rect.Height()) ||
        (scroll_origin_to_expose_rect.Bottom() <
             non_zero_visible_rect.Bottom() &&
         scroll_origin_to_expose_rect.Height() >
             non_zero_visible_rect.Height())) {
      scroll_y = mojom::blink::ScrollAlignment::Behavior::kBottom;
    }
  }

  // We would like calculate the ScrollPosition to move
  // |scroll_origin_to_expose_rect| inside the scroll_snapport, which is based
  // on the scroll_origin of the scroller.
  non_zero_visible_rect.Move(
      -PhysicalOffset::FromVector2dFRound(current_scroll_offset));

  // Given the X behavior, compute the X coordinate.
  float x;
  if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kNoScroll) {
    x = current_scroll_offset.x();
  } else if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kRight) {
    x = (scroll_origin_to_expose_rect.Right() - non_zero_visible_rect.Right())
            .ToFloat();
  } else if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kCenter) {
    x = ((scroll_origin_to_expose_rect.X() +
          scroll_origin_to_expose_rect.Right() -
          (non_zero_visible_rect.X() + non_zero_visible_rect.Right())) /
         2)
            .ToFloat();
  } else {
    x = (scroll_origin_to_expose_rect.X() - non_zero_visible_rect.X())
            .ToFloat();
  }

  // Given the Y behavior, compute the Y coordinate.
  float y;
  if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kNoScroll) {
    y = current_scroll_offset.y();
  } else if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kBottom) {
    y = (scroll_origin_to_expose_rect.Bottom() - non_zero_visible_rect.Bottom())
            .ToFloat();
  } else if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kCenter) {
    y = ((scroll_origin_to_expose_rect.Y() +
          scroll_origin_to_expose_rect.Bottom() -
          (non_zero_visible_rect.Y() + non_zero_visible_rect.Bottom())) /
         2)
            .ToFloat();
  } else {
    y = (scroll_origin_to_expose_rect.Y() - non_zero_visible_rect.Y())
            .ToFloat();
  }

  return ScrollOffset(x, y);
}

mojom::blink::ScrollAlignment PhysicalAlignmentFromSnapAlignStyle(
    const LayoutBox& box,
    ScrollOrientation axis) {
  cc::ScrollSnapAlign snap = box.Style()->GetScrollSnapAlign();
  return ResolveToPhysicalAlignment(
      SnapAlignmentToV8ScrollLogicalPosition(snap.alignment_inline),
      SnapAlignmentToV8ScrollLogicalPosition(snap.alignment_block), axis,
      *box.Style());
}

}  // namespace scroll_into_view_util

}  // namespace blink
