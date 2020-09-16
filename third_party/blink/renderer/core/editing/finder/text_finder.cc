/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/finder/text_finder.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/finder/find_task_controller.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

namespace {

// Returns the element which the beforematch event should be fired on given a
// matching range.
Element* GetBeforematchElement(const Range& range) {
  // Find-in-page matches can't span multiple block-level elements (because
  // the text will be broken by newlines between blocks), so first we find the
  // block-level element which contains the match.
  // This means we only need to traverse up from one node in the range, in
  // this case we are traversing from the start position of the range.
  return EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
}

}  // namespace

TextFinder::FindMatch::FindMatch(Range* range, int ordinal)
    : range_(range), ordinal_(ordinal) {}

void TextFinder::FindMatch::Trace(Visitor* visitor) const {
  visitor->Trace(range_);
}

static void ScrollToVisible(Range* match) {
  const EphemeralRangeInFlatTree range(match);
  const Node& first_node = *match->FirstNode();

  if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled()) {
    // TODO(vmpstr): Rework this, since it is only used for bookkeeping.
    DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(range);

    // We need to update the style and layout since the event dispatched may
    // have modified it, and we need up-to-date layout to ScrollRectToVisible
    // below.
    first_node.GetDocument().UpdateStyleAndLayoutForNode(
        &first_node, DocumentUpdateReason::kFindInPage);
  }
  Settings* settings = first_node.GetDocument().GetSettings();
  bool smooth_find_enabled =
      settings ? settings->GetSmoothScrollForFindEnabled() : false;
  mojom::blink::ScrollBehavior scroll_behavior =
      smooth_find_enabled ? mojom::blink::ScrollBehavior::kSmooth
                          : mojom::blink::ScrollBehavior::kAuto;
  first_node.GetLayoutObject()->ScrollRectToVisible(
      PhysicalRect(match->BoundingBox()),
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
          mojom::blink::ScrollType::kUser,
          true /* make_visible_in_visual_viewport */, scroll_behavior,
          true /* is_for_scroll_sequence */));
  first_node.GetDocument().SetSequentialFocusNavigationStartingPoint(
      const_cast<Node*>(&first_node));
}

bool TextFinder::Find(int identifier,
                      const WebString& search_text,
                      const mojom::blink::FindOptions& options,
                      bool wrap_within_frame,
                      bool* active_now) {
  return FindInternal(identifier, search_text, options, wrap_within_frame,
                      active_now);
}

bool TextFinder::FindInternal(int identifier,
                              const WebString& search_text,
                              const mojom::blink::FindOptions& options,
                              bool wrap_within_frame,
                              bool* active_now,
                              Range* first_match,
                              bool wrapped_around) {
  if (options.new_session) {
    // This find-in-page is redone due to the frame finishing loading.
    // If we can, just reuse the old active match;
    if (options.force && active_match_) {
      should_locate_active_rect_ = true;
      return true;
    }
    UnmarkAllTextMatches();
  } else {
    SetMarkerActive(active_match_.Get(), false);
  }
  if (active_match_ &&
      &active_match_->OwnerDocument() != OwnerFrame().GetFrame()->GetDocument())
    active_match_ = nullptr;

  // If the user has selected something since the last Find operation we want
  // to start from there. Otherwise, we start searching from where the last Find
  // operation left off (either a Find or a FindNext operation).
  // TODO(editing-dev): The use of VisibleSelection should be audited. See
  // crbug.com/657237 for details.
  VisibleSelection selection(
      OwnerFrame().GetFrame()->Selection().ComputeVisibleSelectionInDOMTree());
  bool active_selection = !selection.IsNone();
  if (active_selection) {
    active_match_ = CreateRange(FirstEphemeralRangeOf(selection));
    OwnerFrame().GetFrame()->Selection().Clear();
  }

  DCHECK(OwnerFrame().GetFrame());
  DCHECK(OwnerFrame().GetFrame()->View());
  const FindOptions find_options =
      (options.forward ? 0 : kBackwards) |
      (options.match_case ? 0 : kCaseInsensitive) |
      (wrap_within_frame ? kWrapAround : 0) |
      (options.new_session ? kStartInSelection : 0);
  active_match_ = Editor::FindRangeOfString(
      *OwnerFrame().GetFrame()->GetDocument(), search_text,
      EphemeralRangeInFlatTree(active_match_.Get()), find_options,
      &wrapped_around);

  if (!active_match_) {
    if (current_active_match_frame_ && options.new_session)
      should_locate_active_rect_ = true;
    // In an existing session the next active match might not be in
    // frame.  In this case we don't want to clear the matches cache.
    if (options.new_session)
      ClearFindMatchesCache();

    InvalidatePaintForTickmarks();
    return false;
  }

  // We don't want to search past the same position twice, so if the new match
  // is past the original one and we have wrapped around, then stop now.
  if (first_match && wrapped_around) {
    if (options.forward) {
      // If the start of the new match has gone past the start of the original
      // match, then stop.
      if (ComparePositions(first_match->StartPosition(),
                           active_match_->StartPosition()) <= 0) {
        return false;
      }
    } else {
      // If the end of the new match has gone before the end of the original
      // match, then stop.
      if (ComparePositions(active_match_->EndPosition(),
                           first_match->EndPosition()) <= 0) {
        return false;
      }
    }
  }

  std::unique_ptr<AsyncScrollContext> scroll_context =
      std::make_unique<AsyncScrollContext>();
  scroll_context->identifier = identifier;
  scroll_context->search_text = search_text;
  scroll_context->options = options;
  // Set new_session to false to make sure that subsequent searches are
  // incremental instead of repeatedly finding the same match.
  scroll_context->options.new_session = false;
  scroll_context->wrap_within_frame = wrap_within_frame;
  scroll_context->range = active_match_.Get();
  scroll_context->first_match = first_match ? first_match : active_match_.Get();
  scroll_context->wrapped_around = wrapped_around;
  Element* beforematch_element = GetBeforematchElement(*active_match_);
  scroll_context->was_match_hidden =
      beforematch_element &&
      DisplayLockUtilities::NearestHiddenMatchableInclusiveAncestor(
          *beforematch_element);
  if (options.run_synchronously_for_testing) {
    FireBeforematchEvent(std::move(scroll_context));
  } else {
    scroll_task_.Reset(WTF::Bind(&TextFinder::FireBeforematchEvent,
                                 WrapWeakPersistent(this),
                                 std::move(scroll_context)));
    GetFrame()->GetDocument()->EnqueueAnimationFrameTask(
        scroll_task_.callback());
  }

  bool was_active_frame = current_active_match_frame_;
  current_active_match_frame_ = true;

  bool is_active = SetMarkerActive(active_match_.Get(), true);
  if (active_now)
    *active_now = is_active;

  // Make sure no node is focused. See http://crbug.com/38700.
  OwnerFrame().GetFrame()->GetDocument()->ClearFocusedElement();

  // Set this frame as focused.
  OwnerFrame().ViewImpl()->SetFocusedFrame(&OwnerFrame());

  if (options.new_session || active_selection || !is_active) {
    // This is either an initial Find operation, a Find-next from a new
    // start point due to a selection, or new matches were found during
    // Find-next due to DOM alteration (that couldn't be set as active), so
    // we set the flag to ask the scoping effort to find the active rect for
    // us and report it back to the UI.
    should_locate_active_rect_ = true;
  } else {
    if (!was_active_frame) {
      if (options.forward)
        active_match_index_ = 0;
      else
        active_match_index_ = find_task_controller_->CurrentMatchCount() - 1;
    } else {
      if (options.forward)
        ++active_match_index_;
      else
        --active_match_index_;

      if (active_match_index_ + 1 > find_task_controller_->CurrentMatchCount())
        active_match_index_ = 0;
      else if (active_match_index_ < 0)
        active_match_index_ = find_task_controller_->CurrentMatchCount() - 1;
    }
    gfx::Rect selection_rect = OwnerFrame().GetFrameView()->ConvertToRootFrame(
        active_match_->BoundingBox());
    ReportFindInPageSelection(selection_rect, active_match_index_ + 1,
                              identifier);
  }

  // We found something, so the result of the previous scoping may be outdated.
  find_task_controller_->ResetLastFindRequestCompletedWithNoMatches();

  return true;
}

void TextFinder::ClearActiveFindMatch() {
  current_active_match_frame_ = false;
  SetMarkerActive(active_match_.Get(), false);
  ResetActiveMatch();
}

LocalFrame* TextFinder::GetFrame() const {
  return OwnerFrame().GetFrame();
}

void TextFinder::SetFindEndstateFocusAndSelection() {
  if (!ActiveMatchFrame())
    return;

  Range* active_match = ActiveMatch();
  if (!active_match)
    return;

  // If the user has set the selection since the match was found, we
  // don't focus anything.
  if (!GetFrame()->Selection().GetSelectionInDOMTree().IsNone())
    return;

  // Need to clean out style and layout state before querying
  // Element::isFocusable().
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);

  // Try to find the first focusable node up the chain, which will, for
  // example, focus links if we have found text within the link.
  Node* node = active_match->FirstNode();
  if (node && node->IsInShadowTree()) {
    if (Node* host = node->OwnerShadowHost()) {
      if (IsA<HTMLInputElement>(*host) || IsA<HTMLTextAreaElement>(*host))
        node = host;
    }
  }
  const EphemeralRange active_match_range(active_match);
  if (node) {
    for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*node)) {
      auto* element = DynamicTo<Element>(runner);
      if (!element)
        continue;
      if (element->IsFocusable()) {
        // Found a focusable parent node. Set the active match as the
        // selection and focus to the focusable node.
        GetFrame()->Selection().SetSelectionAndEndTyping(
            SelectionInDOMTree::Builder()
                .SetBaseAndExtent(active_match_range)
                .Build());
        GetFrame()->GetDocument()->SetFocusedElement(
            element, FocusParams(SelectionBehaviorOnFocus::kNone,
                                 mojom::blink::FocusType::kNone, nullptr));
        return;
      }
    }
  }

  // Iterate over all the nodes in the range until we find a focusable node.
  // This, for example, sets focus to the first link if you search for
  // text and text that is within one or more links.
  for (Node& runner : active_match_range.Nodes()) {
    auto* element = DynamicTo<Element>(runner);
    if (!element)
      continue;
    if (element->IsFocusable()) {
      GetFrame()->GetDocument()->SetFocusedElement(
          element, FocusParams(SelectionBehaviorOnFocus::kNone,
                               mojom::blink::FocusType::kNone, nullptr));
      return;
    }
  }

  // No node related to the active match was focusable, so set the
  // active match as the selection (so that when you end the Find session,
  // you'll have the last thing you found highlighted) and make sure that
  // we have nothing focused (otherwise you might have text selected but
  // a link focused, which is weird).
  GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(active_match_range)
          .Build());
  GetFrame()->GetDocument()->ClearFocusedElement();

  // Finally clear the active match, for two reasons:
  // We just finished the find 'session' and we don't want future (potentially
  // unrelated) find 'sessions' operations to start at the same place.
  // The WebLocalFrameImpl could get reused and the activeMatch could end up
  // pointing to a document that is no longer valid. Keeping an invalid
  // reference around is just asking for trouble.
  ResetActiveMatch();
}

void TextFinder::StopFindingAndClearSelection() {
  CancelPendingScopingEffort();

  // Remove all markers for matches found and turn off the highlighting.
  OwnerFrame().GetFrame()->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextMatch());
  OwnerFrame().GetFrame()->GetEditor().SetMarkedTextMatchesAreHighlighted(
      false);
  ClearFindMatchesCache();
  ResetActiveMatch();

  // Let the frame know that we don't want tickmarks anymore.
  InvalidatePaintForTickmarks();

  ReportFindInPageTerminationToAccessibility();
}

void TextFinder::ReportFindInPageTerminationToAccessibility() {
  GetFrame()
      ->GetLocalFrameHostRemote()
      .HandleAccessibilityFindInPageTermination();
}

void TextFinder::ReportFindInPageResultToAccessibility(int identifier) {
  if (!active_match_)
    return;

  auto* ax_object_cache =
      OwnerFrame().GetFrame()->GetDocument()->ExistingAXObjectCache();
  if (!ax_object_cache)
    return;

  Node* start_node = active_match_->startContainer();
  Node* end_node = active_match_->endContainer();
  ax_object_cache->HandleTextMarkerDataAdded(start_node, end_node);

  int32_t start_id = ax_object_cache->GetAXID(start_node);
  int32_t end_id = ax_object_cache->GetAXID(end_node);

  auto params = mojom::blink::FindInPageResultAXParams::New(
      identifier, active_match_index_ + 1, start_id,
      active_match_->startOffset(), end_id, active_match_->endOffset());
  GetFrame()->GetLocalFrameHostRemote().HandleAccessibilityFindInPageResult(
      std::move(params));
}

void TextFinder::StartScopingStringMatches(
    int identifier,
    const WebString& search_text,
    const mojom::blink::FindOptions& options) {
  CancelPendingScopingEffort();

  // This is a brand new search, so we need to reset everything.
  // Scoping is just about to begin.
  scoping_in_progress_ = true;

  // Need to keep the current identifier locally in order to finish the
  // request in case the frame is detached during the process.
  find_request_identifier_ = identifier;

  // Clear highlighting for this frame.
  UnmarkAllTextMatches();

  // Clear the tickmarks and results cache.
  ClearFindMatchesCache();

  // Clear the total match count and increment markers version.
  ResetMatchCount();

  // Clear the counter from last operation.
  next_invalidate_after_ = 0;

  // The view might be null on detached frames.
  LocalFrame* frame = OwnerFrame().GetFrame();
  if (frame && frame->GetPage())
    frame_scoping_ = true;

  find_task_controller_->StartRequest(identifier, search_text, options);
}

void TextFinder::FlushCurrentScopingEffort(int identifier) {
  if (!OwnerFrame().GetFrame() || !OwnerFrame().GetFrame()->GetPage())
    return;

  frame_scoping_ = false;
  IncreaseMatchCount(identifier, 0);
}

void TextFinder::DidFindMatch(int identifier,
                              int current_total_matches,
                              Range* result_range) {
  // Catch a special case where Find found something but doesn't know what
  // the bounding box for it is. In this case we set the first match we find
  // as the active rect.
  bool found_active_match = false;
  if (should_locate_active_rect_) {
    IntRect result_bounds = result_range->BoundingBox();
    IntRect active_selection_rect =
        active_match_.Get() ? active_match_->BoundingBox() : result_bounds;

    // If the Find function found a match it will have stored where the
    // match was found in active_selection_rect_ on the current frame. If we
    // find this rect during scoping it means we have found the active
    // tickmark.
    if (active_selection_rect == result_bounds) {
      // We have found the active tickmark frame.
      current_active_match_frame_ = true;
      found_active_match = true;
      // We also know which tickmark is active now.
      active_match_index_ = current_total_matches - 1;
      // To stop looking for the active tickmark, we set this flag.
      should_locate_active_rect_ = false;

      // Notify browser of new location for the selected rectangle.
      ReportFindInPageSelection(
          OwnerFrame().GetFrameView()->ConvertToRootFrame(result_bounds),
          active_match_index_ + 1, identifier);
    }
  }
  DocumentMarkerController& marker_controller =
      OwnerFrame().GetFrame()->GetDocument()->Markers();
  EphemeralRange ephemeral_result_range(result_range);
  // Scroll() may have added a match marker to this range already.
  if (!marker_controller.FirstMarkerIntersectingEphemeralRange(
          ephemeral_result_range, DocumentMarker::MarkerTypes::TextMatch())) {
    marker_controller.AddTextMatchMarker(
        EphemeralRange(result_range),
        found_active_match ? TextMatchMarker::MatchStatus::kActive
                           : TextMatchMarker::MatchStatus::kInactive);
  }

  find_matches_cache_.push_back(FindMatch(result_range, current_total_matches));
}

void TextFinder::UpdateMatches(int identifier,
                               int found_match_count,
                               bool finished_whole_request) {
  GetFrame()->GetEditor().SetMarkedTextMatchesAreHighlighted(true);

  // Let the frame know how many matches we found during this pass.
  IncreaseMatchCount(identifier, found_match_count);

  // If we found anything during this pass, we should redraw. However, we
  // don't want to spam too much if the page is extremely long, so if we
  // reach a certain point we start throttling the redraw requests.
  if (!finished_whole_request)
    InvalidateIfNecessary();
}

void TextFinder::FinishCurrentScopingEffort(int identifier) {
  scoping_in_progress_ = false;
  if (!OwnerFrame().GetFrame())
    return;

  if (!total_match_count_)
    OwnerFrame().GetFrame()->Selection().Clear();

  FlushCurrentScopingEffort(identifier);
  // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
  InvalidatePaintForTickmarks();
}

void TextFinder::CancelPendingScopingEffort() {
  active_match_index_ = -1;
  scoping_in_progress_ = false;
  find_task_controller_->CancelPendingRequest();
}

void TextFinder::IncreaseMatchCount(int identifier, int count) {
  if (count)
    ++find_match_markers_version_;

  total_match_count_ += count;

  // Update the UI with the latest findings.
  OwnerFrame().GetFindInPage()->ReportFindInPageMatchCount(
      identifier, total_match_count_, !frame_scoping_);
}

void TextFinder::ReportFindInPageSelection(const gfx::Rect& selection_rect,
                                           int active_match_ordinal,
                                           int identifier) {
  // Update the UI with the latest selection rect.
  OwnerFrame().GetFindInPage()->ReportFindInPageSelection(
      identifier, active_match_ordinal, selection_rect,
      false /* final_update */);
  // Update accessibility too, so if the user commits to this query
  // we can move accessibility focus to this result.
  ReportFindInPageResultToAccessibility(identifier);
}

void TextFinder::ResetMatchCount() {
  if (total_match_count_ > 0)
    ++find_match_markers_version_;

  total_match_count_ = 0;
  frame_scoping_ = false;
}

void TextFinder::ClearFindMatchesCache() {
  if (!find_matches_cache_.IsEmpty())
    ++find_match_markers_version_;

  find_matches_cache_.clear();
  find_match_rects_are_valid_ = false;
}

void TextFinder::UpdateFindMatchRects() {
  IntSize current_document_size = OwnerFrame().DocumentSize();
  if (document_size_for_current_find_match_rects_ != current_document_size) {
    document_size_for_current_find_match_rects_ = current_document_size;
    find_match_rects_are_valid_ = false;
  }

  wtf_size_t dead_matches = 0;
  for (FindMatch& match : find_matches_cache_) {
    if (!match.range_->BoundaryPointsValid() ||
        !match.range_->startContainer()->isConnected())
      match.rect_ = FloatRect();
    else if (!find_match_rects_are_valid_)
      match.rect_ = FindInPageRectFromRange(EphemeralRange(match.range_.Get()));

    if (match.rect_.IsEmpty())
      ++dead_matches;
  }

  // Remove any invalid matches from the cache.
  if (dead_matches) {
    HeapVector<FindMatch> filtered_matches;
    filtered_matches.ReserveCapacity(find_matches_cache_.size() - dead_matches);

    for (const FindMatch& match : find_matches_cache_) {
      if (!match.rect_.IsEmpty())
        filtered_matches.push_back(match);
    }

    find_matches_cache_.swap(filtered_matches);
  }

  find_match_rects_are_valid_ = true;
}

gfx::RectF TextFinder::ActiveFindMatchRect() {
  if (!current_active_match_frame_ || !active_match_)
    return gfx::RectF();

  return gfx::RectF(FindInPageRectFromRange(EphemeralRange(ActiveMatch())));
}

Vector<gfx::RectF> TextFinder::FindMatchRects() {
  UpdateFindMatchRects();

  Vector<gfx::RectF> match_rects;
  match_rects.ReserveCapacity(match_rects.size() + find_matches_cache_.size());
  for (const FindMatch& match : find_matches_cache_) {
    DCHECK(!match.rect_.IsEmpty());
    match_rects.push_back(match.rect_);
  }

  return match_rects;
}

int TextFinder::SelectNearestFindMatch(const gfx::PointF& point,
                                       gfx::Rect* selection_rect) {
  int index = NearestFindMatch(FloatPoint(point), nullptr);
  if (index != -1)
    return SelectFindMatch(static_cast<unsigned>(index), selection_rect);

  return -1;
}

int TextFinder::NearestFindMatch(const FloatPoint& point,
                                 float* distance_squared) {
  UpdateFindMatchRects();

  int nearest = -1;
  float nearest_distance_squared = FLT_MAX;
  for (wtf_size_t i = 0; i < find_matches_cache_.size(); ++i) {
    DCHECK(!find_matches_cache_[i].rect_.IsEmpty());
    FloatSize offset = point - find_matches_cache_[i].rect_.Center();
    float width = offset.Width();
    float height = offset.Height();
    float current_distance_squared = width * width + height * height;
    if (current_distance_squared < nearest_distance_squared) {
      nearest = i;
      nearest_distance_squared = current_distance_squared;
    }
  }

  if (distance_squared)
    *distance_squared = nearest_distance_squared;

  return nearest;
}

int TextFinder::SelectFindMatch(unsigned index, gfx::Rect* selection_rect) {
  SECURITY_DCHECK(index < find_matches_cache_.size());

  Range* range = find_matches_cache_[index].range_;
  if (!range->BoundaryPointsValid() || !range->startContainer()->isConnected())
    return -1;

  // Check if the match is already selected.
  if (!current_active_match_frame_ || !active_match_ ||
      !AreRangesEqual(active_match_.Get(), range)) {
    active_match_index_ = find_matches_cache_[index].ordinal_ - 1;

    // Set this frame as the active frame (the one with the active highlight).
    current_active_match_frame_ = true;
    OwnerFrame().ViewImpl()->SetFocusedFrame(&OwnerFrame());

    if (active_match_)
      SetMarkerActive(active_match_.Get(), false);
    active_match_ = range;
    SetMarkerActive(active_match_.Get(), true);

    // Clear any user selection, to make sure Find Next continues on from the
    // match we just activated.
    OwnerFrame().GetFrame()->Selection().Clear();

    // Make sure no node is focused. See http://crbug.com/38700.
    OwnerFrame().GetFrame()->GetDocument()->ClearFocusedElement();
  }

  IntRect active_match_rect;
  IntRect active_match_bounding_box =
      ComputeTextRect(EphemeralRange(active_match_.Get()));

  if (!active_match_bounding_box.IsEmpty()) {
    if (active_match_->FirstNode() &&
        active_match_->FirstNode()->GetLayoutObject()) {
      active_match_->FirstNode()->GetLayoutObject()->ScrollRectToVisible(
          PhysicalRect(active_match_bounding_box),
          ScrollAlignment::CreateScrollIntoViewParams(
              ScrollAlignment::CenterIfNeeded(),
              ScrollAlignment::CenterIfNeeded(),
              mojom::blink::ScrollType::kUser));

      // Absolute coordinates are scroll-variant so the bounding box will change
      // if the page is scrolled by ScrollRectToVisible above. Recompute the
      // bounding box so we have the updated location for the zoom below.
      // TODO(bokan): This should really use the return value from
      // ScrollRectToVisible which returns the updated position of the
      // scrolled rect. However, this was recently added and this is a fix
      // that needs to be merged to a release branch.
      // https://crbug.com/823365.
      active_match_bounding_box =
          ComputeTextRect(EphemeralRange(active_match_.Get()));
    }

    // Zoom to the active match.
    active_match_rect = OwnerFrame().GetFrameView()->ConvertToRootFrame(
        active_match_bounding_box);
    OwnerFrame().LocalRoot()->FrameWidget()->ZoomToFindInPageRect(
        active_match_rect);
  }

  if (selection_rect)
    *selection_rect = active_match_rect;

  return active_match_index_ + 1;
}

TextFinder::TextFinder(WebLocalFrameImpl& owner_frame)
    : owner_frame_(&owner_frame),
      find_task_controller_(
          MakeGarbageCollected<FindTaskController>(owner_frame, *this)),
      current_active_match_frame_(false),
      active_match_index_(-1),
      total_match_count_(-1),
      frame_scoping_(false),
      find_request_identifier_(-1),
      next_invalidate_after_(0),
      find_match_markers_version_(0),
      should_locate_active_rect_(false),
      scoping_in_progress_(false),
      find_match_rects_are_valid_(false) {}

bool TextFinder::SetMarkerActive(Range* range, bool active) {
  if (!range || range->collapsed())
    return false;
  Document* document = OwnerFrame().GetFrame()->GetDocument();
  document->SetFindInPageActiveMatchNode(active ? range->startContainer()
                                                : nullptr);
  return document->Markers().SetTextMatchMarkersActive(EphemeralRange(range),
                                                       active);
}

void TextFinder::UnmarkAllTextMatches() {
  LocalFrame* frame = OwnerFrame().GetFrame();
  if (frame && frame->GetPage() &&
      frame->GetEditor().MarkedTextMatchesAreHighlighted()) {
    frame->GetDocument()->Markers().RemoveMarkersOfTypes(
        DocumentMarker::MarkerTypes::TextMatch());
  }
}

void TextFinder::InvalidateIfNecessary() {
  if (find_task_controller_->CurrentMatchCount() <= next_invalidate_after_)
    return;

  // FIXME: (http://crbug.com/6819) Optimize the drawing of the tickmarks and
  // remove this. This calculation sets a milestone for when next to
  // invalidate the scrollbar and the content area. We do this so that we
  // don't spend too much time drawing the scrollbar over and over again.
  // Basically, up until the first 500 matches there is no throttle.
  // After the first 500 matches, we set set the milestone further and
  // further out (750, 1125, 1688, 2K, 3K).
  static const int kStartSlowingDownAfter = 500;
  static const int kSlowdown = 750;

  int i = find_task_controller_->CurrentMatchCount() / kStartSlowingDownAfter;
  next_invalidate_after_ += i * kSlowdown;
  InvalidatePaintForTickmarks();
}

void TextFinder::FlushCurrentScoping() {
  FlushCurrentScopingEffort(find_request_identifier_);
}

void TextFinder::InvalidatePaintForTickmarks() {
  OwnerFrame().GetFrame()->ContentLayoutObject()->InvalidatePaintForTickmarks();
}

void TextFinder::Trace(Visitor* visitor) const {
  visitor->Trace(owner_frame_);
  visitor->Trace(find_task_controller_);
  visitor->Trace(active_match_);
  visitor->Trace(find_matches_cache_);
}

void TextFinder::FireBeforematchEvent(
    std::unique_ptr<AsyncScrollContext> context) {
  // During the async step, the match may have been removed from the dom.
  if (context->range->collapsed()) {
    // If the range we were going to scroll to was removed, then we should
    // continue to search for the next match.
    // We don't need to worry about the case where another Find has already been
    // initiated, because if it was, then the task to run this would have been
    // canceled.
    active_match_ = context->range;
    FindInternal(context->identifier, context->search_text, context->options,
                 context->wrap_within_frame, /*active_now=*/nullptr,
                 context->first_match, context->wrapped_around);
    return;
  }

  if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
          GetFrame()->GetDocument()->GetExecutionContext())) {
    Element* beforematch_element = GetBeforematchElement(*context->range);
    // Note that we don't check the `range.EndPosition()` since we just activate
    // the beginning of the range. In find-in-page cases, the end position is
    // the same since the matches cannot cross block boundaries. However, in
    // scroll-to-text, the range might be different, but we still just activate
    // the beginning of the range. See
    // https://github.com/WICG/display-locking/issues/125 for more details.
    if (beforematch_element) {
      // If the beforematch event handler causes layout shift, then we should
      // give it layout shift allowance because it is responding to the user
      // initiated find-in-page.
      OwnerFrame()
          .GetFrameView()
          ->GetLayoutShiftTracker()
          .NotifyFindInPageInput();
      beforematch_element->DispatchEvent(
          *Event::CreateBubble(event_type_names::kBeforematch));
    }
    // TODO(jarhar): Consider what to do based on DOM/style modifications made
    // by the beforematch event here and write tests for it once we decide on a
    // behavior here: https://github.com/WICG/display-locking/issues/150
  }

  if (context->options.run_synchronously_for_testing) {
    // We need to update style and layout to account for script modifying
    // dom/style before scrolling when we are running synchronously.
    GetFrame()->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
    Scroll(std::move(context));
  } else {
    scroll_task_.Reset(WTF::Bind(&TextFinder::Scroll, WrapWeakPersistent(this),
                                 std::move(context)));
    GetFrame()->GetDocument()->EnqueueAnimationFrameTask(
        scroll_task_.callback());
  }
}

void TextFinder::Scroll(std::unique_ptr<AsyncScrollContext> context) {
  // The beforematch event, as well as any other script that may have run during
  // the async step, may have removed the matching text from the dom, in which
  // case we shouldn't scroll to it.
  // Likewise, if the target scroll element is display locked, then we shouldn't
  // scroll to it.
  Element* beforematch_element = GetBeforematchElement(*context->range);
  if (context->range->collapsed() ||
      (beforematch_element &&
       DisplayLockUtilities::NearestHiddenMatchableInclusiveAncestor(
           *beforematch_element))) {
    // If the range we were going to scroll to was removed or display locked,
    // then we should continue to search for the next match.
    // We don't need to worry about the case where another Find has already been
    // initiated, because if it was, then the task to run this would have been
    // canceled.
    // We also need to re-assign to active_match_ here in order to make sure the
    // search starts from context->range. active_match_ may have been unassigned
    // during the async steps.
    active_match_ = context->range;
    FindInternal(context->identifier, context->search_text, context->options,
                 context->wrap_within_frame, /*active_now=*/nullptr,
                 context->first_match, context->wrapped_around);
    return;
  }

  if (context->was_match_hidden) {
    GetFrame()
        ->GetDocument()
        ->MarkHasFindInPageBeforematchExpandedHiddenMatchable();
  }

  ScrollToVisible(context->range);

  // If the user is browsing a page with autosizing, adjust the zoom to the
  // column where the next hit has been found. Doing this when autosizing is
  // not set will result in a zoom reset on small devices.
  if (GetFrame()->GetDocument()->GetTextAutosizer()->PageNeedsAutosizing()) {
    OwnerFrame().LocalRoot()->FrameWidget()->ZoomToFindInPageRect(
        OwnerFrame().GetFrameView()->ConvertToRootFrame(
            ComputeTextRect(EphemeralRange(context->range))));
  }

  // DidFindMatch will race against this to add a text match marker to this
  // range. In the case where the match is hidden and the beforematch event (or
  // anything else) reveals the range in between DidFindMatch and this function,
  // we need to add the marker again or else it won't show up at all.
  EphemeralRange ephemeral_range(context->range);
  DocumentMarkerController& marker_controller =
      OwnerFrame().GetFrame()->GetDocument()->Markers();
  if (!context->options.run_synchronously_for_testing &&
      !marker_controller.FirstMarkerIntersectingEphemeralRange(
          ephemeral_range, DocumentMarker::MarkerTypes::TextMatch())) {
    marker_controller.AddTextMatchMarker(ephemeral_range,
                                         TextMatchMarker::MatchStatus::kActive);
    SetMarkerActive(context->range, true);
  }
}

}  // namespace blink
