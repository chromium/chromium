// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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

// Helper to return the parent LayoutBox, crossing local frame boundaries, that
// a scroll should bubble up to or nullptr if the local root has been reached.
// The return optional will be empty if the scroll is blocked from bubbling to
// the root.
absl::optional<LayoutBox*> GetScrollParent(
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
    return absl::nullopt;

  if (!box.GetFrame()->IsLocalRoot()) {
    // The parent is a local iframe, convert to the absolute coordinate space
    // of its document and continue from the owner's LayoutBox.
    HTMLFrameOwnerElement* owner_element = box.GetDocument().LocalOwner();
    DCHECK(owner_element);

    // A display:none iframe can have a LayoutView but its owner element won't
    // have a LayoutObject. If that happens, don't bubble the scroll.
    if (!owner_element->GetLayoutObject())
      return absl::nullopt;

    return owner_element->GetLayoutObject()->EnclosingBox();
  }

  // If the owner is remote, the scroll must continue via IPC.
  DCHECK(box.GetFrame()->IsMainFrame() ||
         box.GetFrame()->Parent()->IsRemoteFrame());
  return nullptr;
}

// Helper that reveals the given rect, given in absolute coordinates, by
// scrolling the given `box` LayoutBox and then all its ancestors up to the
// local root frame.  To continue the reveal through remote ancestors, use
// LayoutObject::ScrollRectToVisible. If the scroll bubbled up to the local
// root successfully, returns the updated absolute rect in the absolute
// coordinates of the local root. Otherwise returns an empty optional.
absl::optional<PhysicalRect> PerformBubblingScrollIntoView(
    const LayoutBox& box,
    const PhysicalRect& absolute_rect,
    mojom::blink::ScrollIntoViewParamsPtr& params,
    bool from_remote_frame) {
  DCHECK(params->type == mojom::blink::ScrollType::kProgrammatic ||
         params->type == mojom::blink::ScrollType::kUser);

  if (!box.GetFrameView())
    return absl::nullopt;

  const LayoutBox* current_box = &box;
  PhysicalRect absolute_rect_to_scroll = absolute_rect;

  // TODO(bokan): Temporary, to track cross-origin scroll-into-view prevalence.
  // https://crbug.com/1339003.
  const SecurityOrigin* starting_frame_origin =
      box.GetFrame()->GetSecurityContext()->GetSecurityOrigin();

  while (current_box) {
    if (absolute_rect_to_scroll.Width() <= 0)
      absolute_rect_to_scroll.SetWidth(LayoutUnit(1));
    if (absolute_rect_to_scroll.Height() <= 0)
      absolute_rect_to_scroll.SetHeight(LayoutUnit(1));

    // If we've reached the main frame's layout viewport (which is always set to
    // the global root scroller, see ViewportScrollCallback::SetScroller), abort
    // if the stop_at_main_frame_layout_viewport option is set. We do this so
    // that we can allow a smooth "scroll and zoom" animation to do the final
    // scroll in cases like scrolling a focused editable box into view.
    // TODO(bokan): Ensure a fenced frame doesn't get a global root scroller
    // and then remove the !IsInFencedFrameTree condition.
    // https://crbug.com/1314858
    if (!current_box->GetFrame()->IsInFencedFrameTree() &&
        params->for_focused_editable && current_box->IsGlobalRootScroller()) {
      break;
    }

    ScrollableArea* area_to_scroll = nullptr;

    if (current_box->IsScrollContainer() && !IsA<LayoutView>(current_box)) {
      area_to_scroll = current_box->GetScrollableArea();
    } else if (!current_box->ContainingBlock()) {
      area_to_scroll = params->make_visible_in_visual_viewport
                           ? current_box->GetFrameView()->GetScrollableArea()
                           : current_box->GetFrameView()->LayoutViewport();
    }

    if (area_to_scroll) {
      ScrollOffset scroll_before = area_to_scroll->GetScrollOffset();
      DCHECK(area_to_scroll->GetSmoothScrollSequencer());
      wtf_size_t num_scroll_sequences =
          area_to_scroll->GetSmoothScrollSequencer()->GetCount();

      absolute_rect_to_scroll =
          area_to_scroll->ScrollIntoView(absolute_rect_to_scroll, params);

      // TODO(bokan): Temporary, to track cross-origin scroll-into-view
      // prevalence. https://crbug.com/1339003.
      // If this is for a scroll sequence, GetScrollOffset won't change until
      // all the animations in the sequence are run which happens
      // asynchronously after this method returns. Thus, for scroll sequences,
      // check instead if an entry was added to the sequence which occurs only
      // if the scroll offset is changed as a result of ScrollIntoView.
      bool scroll_changed =
          params->is_for_scroll_sequence
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
                .ScrollIntoView(absolute_rect_to_scroll, params);
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
    absl::optional<LayoutBox*> next_box_opt =
        GetScrollParent(*current_box, params);
    if (!next_box_opt)
      return absl::nullopt;

    LayoutBox* next_box = *next_box_opt;

    // If the next box to scroll is in another frame, we need to convert the
    // scroll box to the new frame's absolute coordinates.
    if (next_box && next_box->View() != current_box->View()) {
      scroll_into_view_util::ConvertParamsToParentFrame(
          params, gfx::RectF(absolute_rect_to_scroll), *current_box->View(),
          *next_box->View());

      absolute_rect_to_scroll = current_box->View()->LocalToAncestorRect(
          absolute_rect_to_scroll, next_box->View(),
          kTraverseDocumentBoundaries);
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

  frame->GetSmoothScrollSequencer().AbortAnimations();
  frame->GetSmoothScrollSequencer().SetScrollType(params->type);
  params->is_for_scroll_sequence |=
      params->type == mojom::blink::ScrollType::kProgrammatic;

  absl::optional<PhysicalRect> updated_absolute_rect =
      PerformBubblingScrollIntoView(*enclosing_box, absolute_rect, params,
                                    from_remote_frame);

  frame->GetSmoothScrollSequencer().RunQueuedAnimations();

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

}  // namespace scroll_into_view_util

}  // namespace blink
