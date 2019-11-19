/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/selection_controller.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/platform/web_menu_source_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

SelectionController::SelectionController(LocalFrame& frame)
    : frame_(&frame),
      mouse_down_may_start_select_(false),
      mouse_down_was_single_click_in_selection_(false),
      mouse_down_allows_multi_click_(false),
      selection_state_(SelectionState::kHaveNotStartedSelection) {}

void SelectionController::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(original_base_in_flat_tree_);
  DocumentShutdownObserver::Trace(visitor);
}

namespace {

DispatchEventResult DispatchSelectStart(Node* node) {
  if (!node || !node->GetLayoutObject())
    return DispatchEventResult::kNotCanceled;

  return node->DispatchEvent(
      *Event::CreateCancelableBubble(event_type_names::kSelectstart));
}

SelectionInFlatTree ExpandSelectionToRespectUserSelectAll(
    Node* target_node,
    const SelectionInFlatTree& selection) {
  if (selection.IsNone())
    return SelectionInFlatTree();
  Node* const root_user_select_all =
      EditingInFlatTreeStrategy::RootUserSelectAllForNode(target_node);
  if (!root_user_select_all)
    return selection;
  return SelectionInFlatTree::Builder(selection)
      .Collapse(MostBackwardCaretPosition(
          PositionInFlatTree::BeforeNode(*root_user_select_all),
          kCanCrossEditingBoundary))
      .Extend(MostForwardCaretPosition(
          PositionInFlatTree::AfterNode(*root_user_select_all),
          kCanCrossEditingBoundary))
      .Build();
}

static int TextDistance(const PositionInFlatTree& start,
                        const PositionInFlatTree& end) {
  return TextIteratorInFlatTree::RangeLength(
      start, end,
      TextIteratorBehavior::AllVisiblePositionsRangeLengthBehavior());
}

bool CanMouseDownStartSelect(Node* node) {
  if (!node || !node->GetLayoutObject())
    return true;

  if (!node->CanStartSelection())
    return false;

  return true;
}

PositionInFlatTreeWithAffinity PositionWithAffinityOfHitTestResult(
    const HitTestResult& hit_test_result) {
  return FromPositionInDOMTree<EditingInFlatTreeStrategy>(
      hit_test_result.InnerNode()->GetLayoutObject()->PositionForPoint(
          hit_test_result.LocalPoint()));
}

DocumentMarker* SpellCheckMarkerAtPosition(
    DocumentMarkerController& document_marker_controller,
    const PositionInFlatTree& position) {
  return document_marker_controller.FirstMarkerAroundPosition(
      position, DocumentMarker::MarkerTypes::Misspelling());
}

}  // namespace

SelectionInFlatTree AdjustSelectionWithTrailingWhitespace(
    const SelectionInFlatTree& selection) {
  if (selection.IsNone())
    return selection;
  if (!selection.IsRange())
    return selection;
  const bool base_is_first =
      selection.Base() == selection.ComputeStartPosition();
  const PositionInFlatTree& end =
      base_is_first ? selection.Extent() : selection.Base();
  DCHECK_EQ(end, selection.ComputeEndPosition());
  const PositionInFlatTree& new_end = SkipWhitespace(end);
  if (end == new_end)
    return selection;
  if (base_is_first) {
    return SelectionInFlatTree::Builder(selection)
        .SetBaseAndExtent(selection.Base(), new_end)
        .Build();
  }
  return SelectionInFlatTree::Builder(selection)
      .SetBaseAndExtent(new_end, selection.Extent())
      .Build();
}

SelectionController::~SelectionController() = default;

Document& SelectionController::GetDocument() const {
  DCHECK(frame_->GetDocument());
  return *frame_->GetDocument();
}

void SelectionController::ContextDestroyed(Document*) {
  original_base_in_flat_tree_ = PositionInFlatTreeWithAffinity();
}

static PositionInFlatTreeWithAffinity AdjustPositionRespectUserSelectAll(
    Node* inner_node,
    const PositionInFlatTree& selection_start,
    const PositionInFlatTree& selection_end,
    const PositionInFlatTreeWithAffinity& position) {
  const SelectionInFlatTree selection_in_user_select_all =
      CreateVisibleSelection(
          ExpandSelectionToRespectUserSelectAll(
              inner_node,
              position.IsNull()
                  ? SelectionInFlatTree()
                  : SelectionInFlatTree::Builder().Collapse(position).Build()))
          .AsSelection();
  if (!selection_in_user_select_all.IsRange())
    return position;
  if (selection_in_user_select_all.ComputeStartPosition().CompareTo(
          selection_start) < 0) {
    return PositionInFlatTreeWithAffinity(
        selection_in_user_select_all.ComputeStartPosition());
  }
  // TODO(xiaochengh): Do we need to use upstream affinity for end?
  if (selection_end.CompareTo(
          selection_in_user_select_all.ComputeEndPosition()) < 0) {
    return PositionInFlatTreeWithAffinity(
        selection_in_user_select_all.ComputeEndPosition());
  }
  return position;
}

static PositionInFlatTree ComputeStartFromEndForExtendForward(
    const PositionInFlatTree& end,
    TextGranularity granularity) {
  if (granularity == TextGranularity::kCharacter)
    return end;
  // |ComputeStartRespectingGranularity()| returns next word/paragraph for
  // end of word/paragraph position. To get start of word/paragraph at |end|,
  // we pass previous position of |end|.
  return ComputeStartRespectingGranularity(
      PositionInFlatTreeWithAffinity(
          PreviousPositionOf(CreateVisiblePosition(end),
                             kCannotCrossEditingBoundary)
              .DeepEquivalent()),
      granularity);
}

static SelectionInFlatTree ExtendSelectionAsDirectional(
    const PositionInFlatTreeWithAffinity& position,
    const SelectionInFlatTree& selection,
    TextGranularity granularity) {
  DCHECK(!selection.IsNone());
  DCHECK(position.IsNotNull());
  const PositionInFlatTree& start = selection.ComputeStartPosition();
  const PositionInFlatTree& end = selection.ComputeEndPosition();
  const PositionInFlatTree& base = selection.IsBaseFirst() ? start : end;
  if (position.GetPosition() < base) {
    // Extend backward yields backward selection
    //  - forward selection:  *abc ^def ghi| => |abc def^ ghi
    //  - backward selection: *abc |def ghi^ => |abc def ghi^
    const PositionInFlatTree& new_start = ComputeStartRespectingGranularity(
        PositionInFlatTreeWithAffinity(position), granularity);
    const PositionInFlatTree& new_end =
        selection.IsBaseFirst()
            ? ComputeEndRespectingGranularity(
                  new_start, PositionInFlatTreeWithAffinity(start), granularity)
            : end;
    SelectionInFlatTree::Builder builder;
    builder.SetBaseAndExtent(new_end, new_start);
    if (new_start == new_end)
      builder.SetAffinity(position.Affinity());
    return builder.Build();
  }

  // Extend forward yields forward selection
  //  - forward selection:  ^abc def| ghi* => ^abc def ghi|
  //  - backward selection: |abc def^ ghi* => abc ^def ghi|
  const PositionInFlatTree& new_start =
      selection.IsBaseFirst()
          ? start
          : ComputeStartFromEndForExtendForward(end, granularity);
  const PositionInFlatTree& new_end = ComputeEndRespectingGranularity(
      new_start, PositionInFlatTreeWithAffinity(position), granularity);
  SelectionInFlatTree::Builder builder;
  builder.SetBaseAndExtent(new_start, new_end);
  if (new_start == new_end)
    builder.SetAffinity(position.Affinity());
  return builder.Build();
}

static SelectionInFlatTree ExtendSelectionAsNonDirectional(
    const PositionInFlatTree& position,
    const SelectionInFlatTree& selection,
    TextGranularity granularity) {
  DCHECK(!selection.IsNone());
  DCHECK(position.IsNotNull());
  // Shift+Click deselects when selection was created right-to-left
  const PositionInFlatTree& start = selection.ComputeStartPosition();
  const PositionInFlatTree& end = selection.ComputeEndPosition();
  if (start == end && position == start)
    return selection;
  if (position < start) {
    return SelectionInFlatTree::Builder()
        .SetBaseAndExtent(
            end, ComputeStartRespectingGranularity(
                     PositionInFlatTreeWithAffinity(position), granularity))
        .Build();
  }
  if (end < position) {
    return SelectionInFlatTree::Builder()
        .SetBaseAndExtent(
            start,
            ComputeEndRespectingGranularity(
                start, PositionInFlatTreeWithAffinity(position), granularity))
        .Build();
  }
  const int distance_to_start = TextDistance(start, position);
  const int distance_to_end = TextDistance(position, end);
  if (distance_to_start <= distance_to_end) {
    return SelectionInFlatTree::Builder()
        .SetBaseAndExtent(
            end, ComputeStartRespectingGranularity(
                     PositionInFlatTreeWithAffinity(position), granularity))
        .Build();
  }
  return SelectionInFlatTree::Builder()
      .SetBaseAndExtent(
          start,
          ComputeEndRespectingGranularity(
              start, PositionInFlatTreeWithAffinity(position), granularity))
      .Build();
}

// Updating the selection is considered side-effect of the event and so it
// doesn't impact the handled state.
bool SelectionController::HandleSingleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventSingleClick");

  DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());
  Node* inner_node = event.InnerNode();
  if (!(inner_node && inner_node->GetLayoutObject() &&
        mouse_down_may_start_select_))
    return false;

  // Extend the selection if the Shift key is down, unless the click is in a
  // link or image.
  bool extend_selection = IsExtendingSelection(event);

  const PositionInFlatTreeWithAffinity visible_hit_position =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(event.GetHitTestResult()))
          .ToPositionWithAffinity();
  const PositionInFlatTreeWithAffinity& position_to_use =
      visible_hit_position.IsNull()
          ? CreateVisiblePosition(
                PositionInFlatTree::FirstPositionInOrBeforeNode(*inner_node))
                .ToPositionWithAffinity()
          : visible_hit_position;
  const VisibleSelectionInFlatTree& selection =
      this->Selection().ComputeVisibleSelectionInFlatTree();

  // Don't restart the selection when the mouse is pressed on an
  // existing selection so we can allow for text dragging.
  if (LocalFrameView* view = frame_->View()) {
    const PhysicalOffset v_point(view->ConvertFromRootFrame(
        FlooredIntPoint(event.Event().PositionInRootFrame())));
    if (!extend_selection && this->Selection().Contains(v_point)) {
      mouse_down_was_single_click_in_selection_ = true;
      if (!event.Event().FromTouch())
        return false;

      if (HandleTapInsideSelection(event, selection.AsSelection()))
        return false;
    }
  }

  if (extend_selection && !selection.IsNone()) {
    // Note: "fast/events/shift-click-user-select-none.html" makes
    // |pos.isNull()| true.
    const PositionInFlatTreeWithAffinity adjusted_position =
        AdjustPositionRespectUserSelectAll(inner_node, selection.Start(),
                                           selection.End(), position_to_use);
    const TextGranularity granularity = Selection().Granularity();
    if (adjusted_position.IsNull()) {
      UpdateSelectionForMouseDownDispatchingSelectStart(
          inner_node, selection.AsSelection(),
          SetSelectionOptions::Builder().SetGranularity(granularity).Build());
      return false;
    }
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node,
        frame_->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional()
            ? ExtendSelectionAsDirectional(adjusted_position,
                                           selection.AsSelection(), granularity)
            : ExtendSelectionAsNonDirectional(adjusted_position.GetPosition(),
                                              selection.AsSelection(),
                                              granularity),
        SetSelectionOptions::Builder().SetGranularity(granularity).Build());
    return false;
  }

  if (selection_state_ == SelectionState::kExtendedSelection) {
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node, selection.AsSelection(), SetSelectionOptions());
    return false;
  }

  if (position_to_use.IsNull()) {
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node, SelectionInFlatTree(), SetSelectionOptions());
    return false;
  }

  bool is_handle_visible = false;
  const bool has_editable_style = HasEditableStyle(*inner_node);
  if (has_editable_style) {
    const bool is_text_box_empty =
        !RootEditableElement(*inner_node)->HasChildren();
    const bool not_left_click =
        event.Event().button != WebPointerProperties::Button::kLeft;
    if (!is_text_box_empty || not_left_click)
      is_handle_visible = event.Event().FromTouch();
  }

  // This applies the JavaScript selectstart handler, which can change the DOM.
  // SelectionControllerTest_SelectStartHandlerRemovesElement makes this return
  // false.
  if (!UpdateSelectionForMouseDownDispatchingSelectStart(
          inner_node,
          ExpandSelectionToRespectUserSelectAll(
              inner_node,
              SelectionInFlatTree::Builder().Collapse(position_to_use).Build()),
          SetSelectionOptions::Builder()
              .SetShouldShowHandle(is_handle_visible)
              .Build())) {
    // UpdateSelectionForMouseDownDispatchingSelectStart() returns false when
    // the selectstart handler has prevented the default selection behavior from
    // occurring.
    return false;
  }

  // SelectionControllerTest_SetCaretAtHitTestResultWithDisconnectedPosition
  // makes the IsValidFor() check fail.
  if (has_editable_style && event.Event().FromTouch() &&
      position_to_use.IsValidFor(*frame_->GetDocument())) {
    frame_->GetTextSuggestionController().HandlePotentialSuggestionTap(
        position_to_use.GetPosition());
  }

  return false;
}

// Returns true if the tap is processed.
bool SelectionController::HandleTapInsideSelection(
    const MouseEventWithHitTestResults& event,
    const SelectionInFlatTree& selection) {
  if (Selection().ShouldShrinkNextTap()) {
    const bool did_select = SelectClosestWordFromHitTestResult(
        event.GetHitTestResult(), AppendTrailingWhitespace::kDontAppend,
        SelectInputEventType::kTouch);
    if (did_select) {
      frame_->GetEventHandler().ShowNonLocatedContextMenu(
          nullptr, kMenuSourceAdjustSelectionReset);
    }
    return true;
  }

  if (Selection().IsHandleVisible())
    return false;

  const bool did_select = UpdateSelectionForMouseDownDispatchingSelectStart(
      event.InnerNode(), selection,
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  if (did_select) {
    frame_->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                        kMenuSourceTouch);
  }
  return true;
}

// Returns true if selection starts from |SVGText| node and |target_node| is
// not the containing block of |SVGText| node.
// See https://bugs.webkit.org/show_bug.cgi?id=12334 for details.
static bool ShouldRespectSVGTextBoundaries(
    const Node& target_node,
    const FrameSelection& frame_selection) {
  const PositionInFlatTree& base =
      frame_selection.ComputeVisibleSelectionInFlatTree().Base();
  // TODO(editing-dev): We should use |ComputeContainerNode()|.
  const Node* const base_node = base.AnchorNode();
  if (!base_node)
    return false;
  LayoutObject* const base_layout_object = base_node->GetLayoutObject();
  if (!base_layout_object || !base_layout_object->IsSVGText())
    return false;
  return target_node.GetLayoutObject()->ContainingBlock() !=
         base_layout_object->ContainingBlock();
}

void SelectionController::UpdateSelectionForMouseDrag(
    const HitTestResult& hit_test_result,
    const PhysicalOffset& drag_start_pos,
    const PhysicalOffset& last_known_mouse_position) {
  if (!mouse_down_may_start_select_)
    return;

  Node* target = hit_test_result.InnerNode();
  if (!target)
    return;

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout();

  const PositionWithAffinity& raw_target_position =
      Selection().SelectionHasFocus()
          ? PositionRespectingEditingBoundary(
                Selection().ComputeVisibleSelectionInDOMTree().Start(),
                hit_test_result.LocalPoint(), target)
          : PositionWithAffinity();
  const PositionInFlatTreeWithAffinity target_position =
      CreateVisiblePosition(
          FromPositionInDOMTree<EditingInFlatTreeStrategy>(raw_target_position))
          .ToPositionWithAffinity();
  // Don't modify the selection if we're not on a node.
  if (target_position.IsNull())
    return;

  // Restart the selection if this is the first mouse move. This work is usually
  // done in handleMousePressEvent, but not if the mouse press was on an
  // existing selection.

  // Special case to limit selection to the containing block for SVG text.
  // TODO(editing_dev): Isn't there a better non-SVG-specific way to do this?
  if (ShouldRespectSVGTextBoundaries(*target, Selection()))
    return;

  if (selection_state_ == SelectionState::kHaveNotStartedSelection &&
      DispatchSelectStart(target) != DispatchEventResult::kNotCanceled)
    return;

  // |DispatchSelectStart()| can change |GetDocument()| or invalidate
  // target_position by 'selectstart' event handler.
  // TODO(editing-dev): We should also add a regression test when above
  // behaviour happens. See crbug.com/775149.
  if (!Selection().IsAvailable() || !target_position.IsValidFor(GetDocument()))
    return;

  const bool should_extend_selection =
      selection_state_ == SelectionState::kExtendedSelection;
  // Always extend selection here because it's caused by a mouse drag
  selection_state_ = SelectionState::kExtendedSelection;

  const VisibleSelectionInFlatTree& visible_selection =
      Selection().ComputeVisibleSelectionInFlatTree();
  if (visible_selection.IsNone()) {
    // TODO(editing-dev): This is an urgent fix to crbug.com/745501. We should
    // find the root cause and replace this by a proper fix.
    return;
  }

  const PositionInFlatTreeWithAffinity adjusted_position =
      AdjustPositionRespectUserSelectAll(target, visible_selection.Start(),
                                         visible_selection.End(),
                                         target_position);
  const SelectionInFlatTree& adjusted_selection =
      should_extend_selection
          ? ExtendSelectionAsDirectional(adjusted_position,
                                         visible_selection.AsSelection(),
                                         Selection().Granularity())
          : SelectionInFlatTree::Builder().Collapse(adjusted_position).Build();

  // When |adjusted_selection| is caret, it's already canonical. No need to re-
  // canonicalize it.
  const SelectionInFlatTree new_visible_selection =
      adjusted_selection.IsRange()
          ? CreateVisibleSelection(adjusted_selection).AsSelection()
          : adjusted_selection;

  const bool selection_is_directional =
      should_extend_selection ? Selection().IsDirectional() : false;
  SetNonDirectionalSelectionIfNeeded(
      new_visible_selection,
      SetSelectionOptions::Builder()
          .SetGranularity(Selection().Granularity())
          .SetIsDirectional(selection_is_directional)
          .Build(),
      kAdjustEndpointsAtBidiBoundary);
}

bool SelectionController::UpdateSelectionForMouseDownDispatchingSelectStart(
    Node* target_node,
    const SelectionInFlatTree& selection,
    const SetSelectionOptions& set_selection_options) {
  if (target_node && target_node->GetLayoutObject() &&
      !target_node->GetLayoutObject()->IsSelectable())
    return false;

  {
    SelectionInFlatTree::InvalidSelectionResetter resetter(selection);
    if (DispatchSelectStart(target_node) != DispatchEventResult::kNotCanceled)
      return false;
  }

  // |DispatchSelectStart()| can change document hosted by |frame_|.
  if (!this->Selection().IsAvailable())
    return false;

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout();
  const SelectionInFlatTree visible_selection =
      CreateVisibleSelection(selection).AsSelection();

  if (visible_selection.IsRange()) {
    selection_state_ = SelectionState::kExtendedSelection;
    SetNonDirectionalSelectionIfNeeded(visible_selection, set_selection_options,
                                       kDoNotAdjustEndpoints);
    return true;
  }

  selection_state_ = SelectionState::kPlacedCaret;
  SetNonDirectionalSelectionIfNeeded(visible_selection, set_selection_options,
                                     kDoNotAdjustEndpoints);
  return true;
}

bool SelectionController::SelectClosestWordFromHitTestResult(
    const HitTestResult& result,
    AppendTrailingWhitespace append_trailing_whitespace,
    SelectInputEventType select_input_event_type) {
  Node* const inner_node = result.InnerNode();

  if (!inner_node || !inner_node->GetLayoutObject() ||
      !inner_node->GetLayoutObject()->IsSelectable())
    return false;

  // Special-case image local offset to always be zero, to avoid triggering
  // LayoutReplaced::positionFromPoint's advancement of the position at the
  // mid-point of the the image (which was intended for mouse-drag selection
  // and isn't desirable for touch).
  HitTestResult adjusted_hit_test_result = result;
  if (select_input_event_type == SelectInputEventType::kTouch &&
      result.GetImage()) {
    adjusted_hit_test_result.SetNodeAndPosition(result.InnerNode(),
                                                PhysicalOffset());
  }

  const PositionInFlatTreeWithAffinity pos =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(adjusted_hit_test_result))
          .ToPositionWithAffinity();
  const SelectionInFlatTree new_selection =
      pos.IsNotNull()
          ? CreateVisibleSelectionWithGranularity(
                SelectionInFlatTree::Builder().Collapse(pos).Build(),
                TextGranularity::kWord)
                .AsSelection()
          : SelectionInFlatTree();

  // TODO(editing-dev): Fix CreateVisibleSelectionWithGranularity() to not
  // return invalid ranges. Until we do that, we need this check here to avoid a
  // renderer crash when we call PlainText() below (see crbug.com/735774).
  if (new_selection.IsNone() ||
      new_selection.ComputeStartPosition() > new_selection.ComputeEndPosition())
    return false;

  if (select_input_event_type == SelectInputEventType::kTouch) {
    // If node doesn't have text except space, tab or line break, do not
    // select that 'empty' area.
    EphemeralRangeInFlatTree range = new_selection.ComputeRange();
    const String word = PlainText(
        range, TextIteratorBehavior::Builder()
                   .SetEmitsObjectReplacementCharacter(
                       HasEditableStyle(*range.StartPosition().AnchorNode()))
                   .Build());
    if (word.length() >= 1 && word[0] == '\n') {
      // We should not select word from end of line, e.g.
      // "(1)|\n(2)" => "(1)^\n(|2)". See http://crbug.com/974569
      return false;
    }
    if (word.SimplifyWhiteSpace().ContainsOnlyWhitespaceOrEmpty())
      return false;

    Element* const editable =
        RootEditableElementOf(new_selection.ComputeStartPosition());
    if (editable && pos.GetPosition() ==
                        VisiblePositionInFlatTree::LastPositionInNode(*editable)
                            .DeepEquivalent())
      return false;
  }

  const SelectionInFlatTree& adjusted_selection =
      append_trailing_whitespace == AppendTrailingWhitespace::kShouldAppend
          ? AdjustSelectionWithTrailingWhitespace(new_selection)
          : new_selection;

  return UpdateSelectionForMouseDownDispatchingSelectStart(
      inner_node,
      ExpandSelectionToRespectUserSelectAll(inner_node, adjusted_selection),
      SetSelectionOptions::Builder()
          .SetGranularity(TextGranularity::kWord)
          .SetShouldShowHandle(select_input_event_type ==
                               SelectInputEventType::kTouch)
          .Build());
}

void SelectionController::SelectClosestMisspellingFromHitTestResult(
    const HitTestResult& result,
    AppendTrailingWhitespace append_trailing_whitespace) {
  Node* inner_node = result.InnerNode();

  if (!inner_node || !inner_node->GetLayoutObject())
    return;

  const PositionInFlatTreeWithAffinity pos =
      CreateVisiblePosition(PositionWithAffinityOfHitTestResult(result))
          .ToPositionWithAffinity();
  if (pos.IsNull()) {
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node, SelectionInFlatTree(),
        SetSelectionOptions::Builder()
            .SetGranularity(TextGranularity::kWord)
            .Build());
    return;
  }

  const PositionInFlatTree& marker_position =
      pos.GetPosition().ParentAnchoredEquivalent();
  const DocumentMarker* const marker = SpellCheckMarkerAtPosition(
      inner_node->GetDocument().Markers(), marker_position);
  if (!marker) {
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node, SelectionInFlatTree(),
        SetSelectionOptions::Builder()
            .SetGranularity(TextGranularity::kWord)
            .Build());
    return;
  }

  Node* const container_node = marker_position.ComputeContainerNode();
  const PositionInFlatTree start(container_node, marker->StartOffset());
  const PositionInFlatTree end(container_node, marker->EndOffset());
  const SelectionInFlatTree new_selection =
      CreateVisibleSelection(
          SelectionInFlatTree::Builder().Collapse(start).Extend(end).Build())
          .AsSelection();
  const SelectionInFlatTree& adjusted_selection =
      append_trailing_whitespace == AppendTrailingWhitespace::kShouldAppend
          ? AdjustSelectionWithTrailingWhitespace(new_selection)
          : new_selection;
  UpdateSelectionForMouseDownDispatchingSelectStart(
      inner_node,
      ExpandSelectionToRespectUserSelectAll(inner_node, adjusted_selection),
      SetSelectionOptions::Builder()
          .SetGranularity(TextGranularity::kWord)
          .Build());
}

bool SelectionController::SelectClosestWordFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!mouse_down_may_start_select_)
    return false;

  AppendTrailingWhitespace append_trailing_whitespace =
      (result.Event().click_count == 2 &&
       frame_->GetEditor().IsSelectTrailingWhitespaceEnabled())
          ? AppendTrailingWhitespace::kShouldAppend
          : AppendTrailingWhitespace::kDontAppend;

  DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());

  return SelectClosestWordFromHitTestResult(
      result.GetHitTestResult(), append_trailing_whitespace,
      result.Event().FromTouch() ? SelectInputEventType::kTouch
                                 : SelectInputEventType::kMouse);
}

void SelectionController::SelectClosestMisspellingFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!mouse_down_may_start_select_)
    return;

  SelectClosestMisspellingFromHitTestResult(
      result.GetHitTestResult(),
      (result.Event().click_count == 2 &&
       frame_->GetEditor().IsSelectTrailingWhitespaceEnabled())
          ? AppendTrailingWhitespace::kShouldAppend
          : AppendTrailingWhitespace::kDontAppend);
}

void SelectionController::SelectClosestWordOrLinkFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!result.GetHitTestResult().IsLiveLink()) {
    SelectClosestWordFromMouseEvent(result);
    return;
  }

  Node* const inner_node = result.InnerNode();

  if (!inner_node || !inner_node->GetLayoutObject() ||
      !mouse_down_may_start_select_)
    return;

  Element* url_element = result.GetHitTestResult().URLElement();
  const PositionInFlatTreeWithAffinity pos =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(result.GetHitTestResult()))
          .ToPositionWithAffinity();
  const SelectionInFlatTree& new_selection =
      pos.IsNotNull() && pos.AnchorNode()->IsDescendantOf(url_element)
          ? SelectionInFlatTree::Builder()
                .SelectAllChildren(*url_element)
                .Build()
          : SelectionInFlatTree();

  UpdateSelectionForMouseDownDispatchingSelectStart(
      inner_node,
      ExpandSelectionToRespectUserSelectAll(inner_node, new_selection),
      SetSelectionOptions::Builder()
          .SetGranularity(TextGranularity::kWord)
          .Build());
}

// TODO(yosin): We should take |granularity| and |handleVisibility| from
// |newSelection|.
// We should rename this function to appropriate name because
// set_selection_options has selection directional value in few cases.
void SelectionController::SetNonDirectionalSelectionIfNeeded(
    const SelectionInFlatTree& new_selection,
    const SetSelectionOptions& set_selection_options,
    EndPointsAdjustmentMode endpoints_adjustment_mode) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  // TODO(editing-dev): We should use |PositionWithAffinity| to pass affinity
  // to |CreateVisiblePosition()| for |original_base|.
  const PositionInFlatTree& base_position =
      original_base_in_flat_tree_.GetPosition();
  const PositionInFlatTreeWithAffinity original_base =
      base_position.IsConnected()
          ? CreateVisiblePosition(base_position).ToPositionWithAffinity()
          : PositionInFlatTreeWithAffinity();
  const PositionInFlatTreeWithAffinity base =
      original_base.IsNotNull() ? original_base
                                : CreateVisiblePosition(new_selection.Base())
                                      .ToPositionWithAffinity();
  const PositionInFlatTreeWithAffinity extent =
      CreateVisiblePosition(new_selection.Extent()).ToPositionWithAffinity();
  const SelectionInFlatTree& adjusted_selection =
      endpoints_adjustment_mode == kAdjustEndpointsAtBidiBoundary
          ? BidiAdjustment::AdjustForRangeSelection(base, extent)
          : SelectionInFlatTree::Builder()
                .SetBaseAndExtent(base.GetPosition(), extent.GetPosition())
                .Build();

  SelectionInFlatTree::Builder builder(new_selection);
  if (adjusted_selection.Base() != base.GetPosition() ||
      adjusted_selection.Extent() != extent.GetPosition()) {
    original_base_in_flat_tree_ = base;
    SetContext(&GetDocument());
    builder.SetBaseAndExtent(adjusted_selection.Base(),
                             adjusted_selection.Extent());
  } else if (original_base.IsNotNull()) {
    if (CreateVisiblePosition(
            Selection().ComputeVisibleSelectionInFlatTree().Base())
            .DeepEquivalent() ==
        CreateVisiblePosition(new_selection.Base()).DeepEquivalent()) {
      builder.SetBaseAndExtent(original_base.GetPosition(),
                               new_selection.Extent());
    }
    original_base_in_flat_tree_ = PositionInFlatTreeWithAffinity();
  }

  const bool selection_is_directional =
      frame_->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional() ||
      set_selection_options.IsDirectional();
  const SelectionInFlatTree& selection_in_flat_tree = builder.Build();

  const bool selection_remains_the_same =
      Selection().ComputeVisibleSelectionInFlatTree() ==
          CreateVisibleSelection(selection_in_flat_tree) &&
      Selection().IsHandleVisible() ==
          set_selection_options.ShouldShowHandle() &&
      selection_is_directional == Selection().IsDirectional();

  // If selection has not changed we do not clear editing style.
  if (selection_remains_the_same)
    return;
  Selection().SetSelection(
      ConvertToSelectionInDOMTree(selection_in_flat_tree),
      SetSelectionOptions::Builder(set_selection_options)
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetIsDirectional(selection_is_directional)
          .SetCursorAlignOnScroll(CursorAlignOnScroll::kIfNeeded)
          .Build());
}

void SelectionController::SetCaretAtHitTestResult(
    const HitTestResult& hit_test_result) {
  Node* inner_node = hit_test_result.InnerNode();
  DCHECK(inner_node);
  const PositionInFlatTreeWithAffinity visible_hit_pos =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(hit_test_result))
          .ToPositionWithAffinity();
  const PositionInFlatTreeWithAffinity visible_pos =
      visible_hit_pos.IsNull()
          ? CreateVisiblePosition(
                PositionInFlatTree::FirstPositionInOrBeforeNode(*inner_node))
                .ToPositionWithAffinity()
          : visible_hit_pos;

  if (visible_pos.IsNull()) {
    UpdateSelectionForMouseDownDispatchingSelectStart(
        inner_node, SelectionInFlatTree(),
        SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
    return;
  }
  UpdateSelectionForMouseDownDispatchingSelectStart(
      inner_node,
      ExpandSelectionToRespectUserSelectAll(
          inner_node,
          SelectionInFlatTree::Builder().Collapse(visible_pos).Build()),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
}

bool SelectionController::HandleDoubleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventDoubleClick");

  if (!Selection().IsAvailable())
    return false;

  if (!mouse_down_allows_multi_click_)
    return HandleSingleClick(event);

  if (event.Event().button != WebPointerProperties::Button::kLeft)
    return false;

  if (Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange()) {
    // A double-click when range is already selected
    // should not change the selection.  So, do not call
    // SelectClosestWordFromMouseEvent, but do set
    // began_selecting_text_ to prevent HandleMouseReleaseEvent
    // from setting caret selection.
    selection_state_ = SelectionState::kExtendedSelection;
    return true;
  }
  if (!SelectClosestWordFromMouseEvent(event))
    return true;
  if (!Selection().IsHandleVisible())
    return true;
  frame_->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                      kMenuSourceTouch);
  return true;
}

bool SelectionController::HandleTripleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventTripleClick");

  if (!Selection().IsAvailable()) {
    // editing/shadow/doubleclick-on-meter-in-shadow-crash.html reach here.
    return false;
  }

  if (!mouse_down_allows_multi_click_)
    return HandleSingleClick(event);

  if (event.Event().button != WebPointerProperties::Button::kLeft)
    return false;

  Node* const inner_node = event.InnerNode();
  if (!(inner_node && inner_node->GetLayoutObject() &&
        mouse_down_may_start_select_))
    return false;

  const PositionInFlatTreeWithAffinity pos =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(event.GetHitTestResult()))
          .ToPositionWithAffinity();
  const SelectionInFlatTree new_selection =
      pos.IsNotNull()
          ? CreateVisibleSelectionWithGranularity(
                SelectionInFlatTree::Builder().Collapse(pos).Build(),
                TextGranularity::kParagraph)
                .AsSelection()
          : SelectionInFlatTree();

  const bool is_handle_visible =
      event.Event().FromTouch() && new_selection.IsRange();

  const bool did_select = UpdateSelectionForMouseDownDispatchingSelectStart(
      inner_node,
      ExpandSelectionToRespectUserSelectAll(inner_node, new_selection),

      SetSelectionOptions::Builder()
          .SetGranularity(TextGranularity::kParagraph)
          .SetShouldShowHandle(is_handle_visible)
          .Build());
  if (!did_select)
    return false;

  if (!Selection().IsHandleVisible())
    return true;
  frame_->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                      kMenuSourceTouch);
  return true;
}

bool SelectionController::HandleMousePressEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "SelectionController::handleMousePressEvent");

  // If we got the event back, that must mean it wasn't prevented,
  // so it's allowed to start a drag or selection if it wasn't in a scrollbar.
  mouse_down_may_start_select_ = (CanMouseDownStartSelect(event.InnerNode()) ||
                                  IsSelectionOverLink(event)) &&
                                 !event.GetScrollbar();
  mouse_down_was_single_click_in_selection_ = false;
  if (!Selection().IsAvailable()) {
    // "gesture-tap-frame-removed.html" reaches here.
    mouse_down_allows_multi_click_ = !event.Event().FromTouch();
  } else {
    // Avoid double-tap touch gesture confusion by restricting multi-click side
    // effects, e.g., word selection, to editable regions.
    mouse_down_allows_multi_click_ =
        !event.Event().FromTouch() ||
        IsEditablePosition(
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());
  }

  if (event.Event().click_count >= 3)
    return HandleTripleClick(event);
  if (event.Event().click_count == 2)
    return HandleDoubleClick(event);
  return HandleSingleClick(event);
}

void SelectionController::HandleMouseDraggedEvent(
    const MouseEventWithHitTestResults& event,
    const IntPoint& mouse_down_pos,
    const PhysicalOffset& drag_start_pos,
    const PhysicalOffset& last_known_mouse_position) {
  TRACE_EVENT0("blink", "SelectionController::handleMouseDraggedEvent");

  if (!Selection().IsAvailable())
    return;
  if (selection_state_ != SelectionState::kExtendedSelection) {
    HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                           HitTestRequest::kRetargetForInert);
    HitTestLocation location(mouse_down_pos);
    HitTestResult result(request, location);
    frame_->GetDocument()->GetLayoutView()->HitTest(location, result);

    UpdateSelectionForMouseDrag(result, drag_start_pos,
                                last_known_mouse_position);
  }
  UpdateSelectionForMouseDrag(event.GetHitTestResult(), drag_start_pos,
                              last_known_mouse_position);
}

void SelectionController::UpdateSelectionForMouseDrag(
    const PhysicalOffset& drag_start_pos,
    const PhysicalOffset& last_known_mouse_position) {
  LocalFrameView* view = frame_->View();
  if (!view)
    return;
  LayoutView* layout_view = frame_->ContentLayoutObject();
  if (!layout_view)
    return;

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                         HitTestRequest::kMove |
                         HitTestRequest::kRetargetForInert);
  HitTestLocation location(view->ViewportToFrame(last_known_mouse_position));
  HitTestResult result(request, location);
  layout_view->HitTest(location, result);
  UpdateSelectionForMouseDrag(result, drag_start_pos,
                              last_known_mouse_position);
}

bool SelectionController::HandleMouseReleaseEvent(
    const MouseEventWithHitTestResults& event,
    const PhysicalOffset& drag_start_pos) {
  TRACE_EVENT0("blink", "SelectionController::handleMouseReleaseEvent");

  if (!Selection().IsAvailable())
    return false;

  bool handled = false;
  mouse_down_may_start_select_ = false;
  // Clear the selection if the mouse didn't move after the last mouse
  // press and it's not a context menu click.  We do this so when clicking
  // on the selection, the selection goes away.  However, if we are
  // editing, place the caret.
  if (mouse_down_was_single_click_in_selection_ &&
      selection_state_ != SelectionState::kExtendedSelection &&
      drag_start_pos == PhysicalOffset(FlooredIntPoint(
                            event.Event().PositionInRootFrame())) &&
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange() &&
      event.Event().button != WebPointerProperties::Button::kRight) {
    // TODO(editing-dev): Use of UpdateStyleAndLayout
    // needs to be audited.  See http://crbug.com/590369 for more details.
    frame_->GetDocument()->UpdateStyleAndLayout();

    SelectionInFlatTree::Builder builder;
    Node* node = event.InnerNode();
    if (node && node->GetLayoutObject() && HasEditableStyle(*node)) {
      const PositionInFlatTreeWithAffinity pos =
          CreateVisiblePosition(
              PositionWithAffinityOfHitTestResult(event.GetHitTestResult()))
              .ToPositionWithAffinity();
      if (pos.IsNotNull())
        builder.Collapse(pos);
    }

    const SelectionInFlatTree new_selection = builder.Build();
    if (Selection().ComputeVisibleSelectionInFlatTree() !=
        CreateVisibleSelection(new_selection)) {
      Selection().SetSelectionAndEndTyping(
          ConvertToSelectionInDOMTree(new_selection));
    }

    handled = true;
  }

  Selection().NotifyTextControlOfSelectionChange(SetSelectionBy::kUser);

  Selection().SelectFrameElementInParentIfFullySelected();

  if (event.Event().button == WebPointerProperties::Button::kMiddle &&
      !event.IsOverLink()) {
    // Ignore handled, since we want to paste to where the caret was placed
    // anyway.
    handled = HandlePasteGlobalSelection(event.Event()) || handled;
  }

  return handled;
}

bool SelectionController::HandlePasteGlobalSelection(
    const WebMouseEvent& mouse_event) {
  // If the event was a middle click, attempt to copy global selection in after
  // the newly set caret position.
  //
  // This code is called from either the mouse up or mouse down handling. There
  // is some debate about when the global selection is pasted:
  //   xterm: pastes on up.
  //   GTK: pastes on down.
  //   Qt: pastes on up.
  //   Firefox: pastes on up.
  //   Chromium: pastes on up.
  //
  // There is something of a webcompat angle to this well, as highlighted by
  // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
  // down then the text is pasted just before the onclick handler runs and
  // clears the text box. So it's important this happens after the event
  // handlers have been fired.
  if (mouse_event.GetType() != WebInputEvent::kMouseUp)
    return false;

  if (!frame_->GetPage())
    return false;
  Frame* focus_frame =
      frame_->GetPage()->GetFocusController().FocusedOrMainFrame();
  // Do not paste here if the focus was moved somewhere else.
  if (frame_ == focus_frame)
    return frame_->GetEditor().ExecuteCommand("PasteGlobalSelection");

  return false;
}

bool SelectionController::HandleGestureLongPress(
    const HitTestResult& hit_test_result) {
  TRACE_EVENT0("blink", "SelectionController::handleGestureLongPress");

  if (!Selection().IsAvailable())
    return false;
  if (hit_test_result.IsLiveLink())
    return false;

  Node* inner_node = hit_test_result.InnerNode();
  inner_node->GetDocument().UpdateStyleAndLayoutTree();
  bool inner_node_is_selectable = HasEditableStyle(*inner_node) ||
                                  inner_node->IsTextNode() ||
                                  inner_node->CanStartSelection();
  if (!inner_node_is_selectable)
    return false;

  if (SelectClosestWordFromHitTestResult(hit_test_result,
                                         AppendTrailingWhitespace::kDontAppend,
                                         SelectInputEventType::kTouch))
    return Selection().IsAvailable();

  if (!inner_node->isConnected() || !inner_node->GetLayoutObject())
    return false;
  SetCaretAtHitTestResult(hit_test_result);
  return false;
}

void SelectionController::HandleGestureTwoFingerTap(
    const GestureEventWithHitTestResults& targeted_event) {
  TRACE_EVENT0("blink", "SelectionController::handleGestureTwoFingerTap");

  SetCaretAtHitTestResult(targeted_event.GetHitTestResult());
}

void SelectionController::HandleGestureLongTap(
    const GestureEventWithHitTestResults& targeted_event) {
  TRACE_EVENT0("blink", "SelectionController::handleGestureLongTap");

  SetCaretAtHitTestResult(targeted_event.GetHitTestResult());
}

static bool HitTestResultIsMisspelled(const HitTestResult& result) {
  Node* inner_node = result.InnerNode();
  if (!inner_node || !inner_node->GetLayoutObject())
    return false;
  PositionWithAffinity pos_with_affinity =
      inner_node->GetLayoutObject()->PositionForPoint(result.LocalPoint());
  if (pos_with_affinity.IsNull())
    return false;
  // TODO(xiaochengh): Don't use |ParentAnchoredEquivalent()|.
  const Position marker_position =
      pos_with_affinity.GetPosition().ParentAnchoredEquivalent();
  if (!SpellChecker::IsSpellCheckingEnabledAt(marker_position))
    return false;
  return SpellCheckMarkerAtPosition(inner_node->GetDocument().Markers(),
                                    ToPositionInFlatTree(marker_position));
}

void SelectionController::SendContextMenuEvent(
    const MouseEventWithHitTestResults& mev,
    const PhysicalOffset& position) {
  if (!Selection().IsAvailable())
    return;
  if (Selection().Contains(position) || mev.GetScrollbar() ||
      // FIXME: In the editable case, word selection sometimes selects content
      // that isn't underneath the mouse.
      // If the selection is non-editable, we do word selection to make it
      // easier to use the contextual menu items available for text selections.
      // But only if we're above text.
      !(Selection()
            .ComputeVisibleSelectionInDOMTreeDeprecated()
            .IsContentEditable() ||
        (mev.InnerNode() && mev.InnerNode()->IsTextNode())))
    return;

  // Context menu events are always allowed to perform a selection.
  base::AutoReset<bool> mouse_down_may_start_select_change(
      &mouse_down_may_start_select_, true);

  if (mev.Event().menu_source_type != kMenuSourceTouchHandle &&
      HitTestResultIsMisspelled(mev.GetHitTestResult()))
    return SelectClosestMisspellingFromMouseEvent(mev);

  if (!frame_->GetEditor().Behavior().ShouldSelectOnContextualMenuClick())
    return;

  SelectClosestWordOrLinkFromMouseEvent(mev);
}

void SelectionController::PassMousePressEventToSubframe(
    const MouseEventWithHitTestResults& mev) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout();

  // If we're clicking into a frame that is selected, the frame will appear
  // greyed out even though we're clicking on the selection.  This looks
  // really strange (having the whole frame be greyed out), so we deselect the
  // selection.
  PhysicalOffset p(frame_->View()->ConvertFromRootFrame(
      FlooredIntPoint(mev.Event().PositionInRootFrame())));
  if (!Selection().Contains(p))
    return;

  const PositionInFlatTreeWithAffinity visible_pos =
      CreateVisiblePosition(
          PositionWithAffinityOfHitTestResult(mev.GetHitTestResult()))
          .ToPositionWithAffinity();
  if (visible_pos.IsNull()) {
    Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
    return;
  }
  Selection().SetSelectionAndEndTyping(ConvertToSelectionInDOMTree(
      SelectionInFlatTree::Builder().Collapse(visible_pos).Build()));
}

void SelectionController::InitializeSelectionState() {
  selection_state_ = SelectionState::kHaveNotStartedSelection;
}

void SelectionController::SetMouseDownMayStartSelect(bool may_start_select) {
  mouse_down_may_start_select_ = may_start_select;
}

bool SelectionController::MouseDownMayStartSelect() const {
  return mouse_down_may_start_select_;
}

bool SelectionController::MouseDownWasSingleClickInSelection() const {
  return mouse_down_was_single_click_in_selection_;
}

void SelectionController::NotifySelectionChanged() {
  // To avoid regression on speedometer benchmark[1] test, we should not
  // update layout tree in this code block.
  // [1] http://browserbench.org/Speedometer/
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

  const SelectionInDOMTree& selection =
      this->Selection().GetSelectionInDOMTree();
  switch (selection.Type()) {
    case kNoSelection:
      selection_state_ = SelectionState::kHaveNotStartedSelection;
      return;
    case kCaretSelection:
      selection_state_ = SelectionState::kPlacedCaret;
      return;
    case kRangeSelection:
      selection_state_ = SelectionState::kExtendedSelection;
      return;
  }
  NOTREACHED() << "We should handle all SelectionType" << selection;
}

FrameSelection& SelectionController::Selection() const {
  return frame_->Selection();
}

bool IsSelectionOverLink(const MouseEventWithHitTestResults& event) {
  return (event.Event().GetModifiers() & WebInputEvent::Modifiers::kAltKey) !=
             0 &&
         event.IsOverLink();
}

bool IsUserNodeDraggable(const MouseEventWithHitTestResults& event) {
  Node* inner_node = event.InnerNode();

  // TODO(huangdarwin): event.InnerNode() should never be nullptr, but unit
  // tests WebFrameTest.FrameWidgetTest and WebViewTest.ClientTapHandling fail
  // without a nullptr check, as they don't set the InnerNode() appropriately.
  // Remove the if statement nullptr check when those tests are fixed.
  if (!inner_node)
    return false;

  const ComputedStyle* kStyle = inner_node->GetComputedStyle();
  return kStyle && kStyle->UserDrag() == EUserDrag::kElement;
}

bool IsExtendingSelection(const MouseEventWithHitTestResults& event) {
  bool is_mouse_down_on_link_or_image =
      event.IsOverLink() || event.GetHitTestResult().GetImage();

  return (event.Event().GetModifiers() & WebInputEvent::Modifiers::kShiftKey) !=
             0 &&
         !is_mouse_down_on_link_or_image && !IsUserNodeDraggable(event);
}

}  // namespace blink
