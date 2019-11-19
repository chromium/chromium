/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/dom_selection.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static Node* SelectionShadowAncestor(LocalFrame* frame) {
  Node* node = frame->Selection()
                   .ComputeVisibleSelectionInDOMTreeDeprecated()
                   .Base()
                   .AnchorNode();
  if (!node)
    return nullptr;

  if (!node->IsInShadowTree())
    return nullptr;

  return frame->GetDocument()->AncestorInThisScope(node);
}

DOMSelection::DOMSelection(const TreeScope* tree_scope)
    : ContextClient(tree_scope->RootNode().GetDocument().GetFrame()),
      tree_scope_(tree_scope) {}

void DOMSelection::ClearTreeScope() {
  tree_scope_ = nullptr;
}

// TODO(editing-dev): The behavior after loosing browsing context is not
// specified. https://github.com/w3c/selection-api/issues/82
bool DOMSelection::IsAvailable() const {
  return GetFrame() && GetFrame()->Selection().IsAvailable();
}

void DOMSelection::UpdateFrameSelection(
    const SelectionInDOMTree& selection,
    Range* new_cached_range,
    const SetSelectionOptions& passed_options) const {
  DCHECK(GetFrame());
  FrameSelection& frame_selection = GetFrame()->Selection();
  SetSelectionOptions::Builder builder(passed_options);
  builder.SetShouldCloseTyping(true).SetShouldClearTypingStyle(true);
  SetSelectionOptions options = builder.Build();
  // TODO(tkent): Specify FrameSelection::DoNotSetFocus. crbug.com/690272
  const bool did_set =
      frame_selection.SetSelectionDeprecated(selection, options);
  CacheRangeIfSelectionOfDocument(new_cached_range);
  if (!did_set)
    return;
  Element* focused_element = GetFrame()->GetDocument()->FocusedElement();
  frame_selection.DidSetSelectionDeprecated(options);
  if (GetFrame() && GetFrame()->GetDocument() &&
      focused_element != GetFrame()->GetDocument()->FocusedElement()) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      WebFeature::kSelectionFuncionsChangeFocus);
  }
}

VisibleSelection DOMSelection::GetVisibleSelection() const {
  DCHECK(GetFrame());
  return GetFrame()->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
}

bool DOMSelection::IsBaseFirstInSelection() const {
  DCHECK(GetFrame());
  const SelectionInDOMTree& selection =
      GetFrame()->Selection().GetSelectionInDOMTree();
  return selection.IsBaseFirst();
}

// TODO(tkent): Following four functions based on VisibleSelection should be
// removed.
static Position AnchorPosition(const VisibleSelection& selection) {
  Position anchor =
      selection.IsBaseFirst() ? selection.Start() : selection.End();
  return anchor.ParentAnchoredEquivalent();
}

static Position FocusPosition(const VisibleSelection& selection) {
  Position focus =
      selection.IsBaseFirst() ? selection.End() : selection.Start();
  return focus.ParentAnchoredEquivalent();
}

Node* DOMSelection::anchorNode() const {
  if (Range* range = PrimaryRangeOrNull()) {
    if (!GetFrame() || IsBaseFirstInSelection())
      return range->startContainer();
    return range->endContainer();
  }
  return nullptr;
}

unsigned DOMSelection::anchorOffset() const {
  if (Range* range = PrimaryRangeOrNull()) {
    if (!GetFrame() || IsBaseFirstInSelection())
      return range->startOffset();
    return range->endOffset();
  }
  return 0;
}

Node* DOMSelection::focusNode() const {
  if (Range* range = PrimaryRangeOrNull()) {
    if (!GetFrame() || IsBaseFirstInSelection())
      return range->endContainer();
    return range->startContainer();
  }
  return nullptr;
}

unsigned DOMSelection::focusOffset() const {
  if (Range* range = PrimaryRangeOrNull()) {
    if (!GetFrame() || IsBaseFirstInSelection())
      return range->endOffset();
    return range->startOffset();
  }
  return 0;
}

Node* DOMSelection::baseNode() const {
  return anchorNode();
}

unsigned DOMSelection::baseOffset() const {
  return anchorOffset();
}

Node* DOMSelection::extentNode() const {
  return focusNode();
}

unsigned DOMSelection::extentOffset() const {
  return focusOffset();
}

bool DOMSelection::isCollapsed() const {
  if (!IsAvailable() || SelectionShadowAncestor(GetFrame()))
    return true;
  if (Range* range = PrimaryRangeOrNull())
    return range->collapsed();
  return true;
}

String DOMSelection::type() const {
  if (!IsAvailable())
    return String();
  // This is a WebKit DOM extension, incompatible with an IE extension
  // IE has this same attribute, but returns "none", "text" and "control"
  // http://msdn.microsoft.com/en-us/library/ms534692(VS.85).aspx
  if (rangeCount() == 0)
    return "None";
  // Do not use isCollapsed() here. We'd like to return "Range" for
  // range-selection in text control elements.
  if (GetFrame()->Selection().GetSelectionInDOMTree().IsCaret())
    return "Caret";
  return "Range";
}

unsigned DOMSelection::rangeCount() const {
  if (!IsAvailable())
    return 0;
  if (DocumentCachedRange())
    return 1;
  if (GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone())
    return 0;
  // Any selection can be adjusted to Range for Document.
  if (IsSelectionOfDocument())
    return 1;
  // In ShadowRoot, we need to try adjustment.
  if (CreateRangeFromSelectionEditor().IsNotNull())
    return 1;
  return 0;
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapse
void DOMSelection::collapse(Node* node,
                            unsigned offset,
                            ExceptionState& exception_state) {
  if (!IsAvailable())
    return;

  // 1. If node is null, this method must behave identically as
  // removeAllRanges() and abort these steps.
  if (!node) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      WebFeature::kSelectionCollapseNull);
    GetFrame()->Selection().Clear();
    return;
  }

  // 2. The method must throw an IndexSizeError exception if offset is longer
  // than node's length ([DOM4]) and abort these steps.
  Range::CheckNodeWOffset(node, offset, exception_state);
  if (exception_state.HadException())
    return;

  // 3. If node's root is not the document associated with the context object,
  // abort these steps.
  if (!IsValidForPosition(node))
    return;

  // 4. Otherwise, let newRange be a new range.
  Range* new_range = Range::Create(*GetFrame()->GetDocument());

  // 5. Set ([DOM4]) the start and the end of newRange to (node, offset).
  new_range->setStart(node, offset, exception_state);
  if (exception_state.HadException()) {
    new_range->Dispose();
    return;
  }
  new_range->setEnd(node, offset, exception_state);
  if (exception_state.HadException()) {
    new_range->Dispose();
    return;
  }

  // 6. Set the context object's range to newRange.
  UpdateFrameSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(node, offset))
          .Build(),
      new_range,
      SetSelectionOptions::Builder()
          .SetIsDirectional(GetFrame()->Selection().IsDirectional())
          .Build());
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapsetoend
void DOMSelection::collapseToEnd(ExceptionState& exception_state) {
  if (!IsAvailable())
    return;

  // The method must throw InvalidStateError exception if the context object is
  // empty.
  if (rangeCount() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "there is no selection.");
    return;
  }

  if (Range* current_range = DocumentCachedRange()) {
    // Otherwise, it must create a new range, set both its start and end to the
    // end of the context object's range,
    Range* new_range = current_range->cloneRange();
    new_range->collapse(false);

    // and then set the context object's range to the newly-created range.
    SelectionInDOMTree::Builder builder;
    builder.Collapse(new_range->EndPosition());
    UpdateFrameSelection(builder.Build(), new_range, SetSelectionOptions());
  } else {
    // TODO(tkent): The Selection API doesn't define this behavior. We should
    // discuss this on https://github.com/w3c/selection-api/issues/83.
    SelectionInDOMTree::Builder builder;
    builder.Collapse(
        GetFrame()->Selection().GetSelectionInDOMTree().ComputeEndPosition());
    UpdateFrameSelection(builder.Build(), nullptr, SetSelectionOptions());
  }
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapsetostart
void DOMSelection::collapseToStart(ExceptionState& exception_state) {
  if (!IsAvailable())
    return;

  // The method must throw InvalidStateError ([DOM4]) exception if the context
  // object is empty.
  if (rangeCount() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "there is no selection.");
    return;
  }

  if (Range* current_range = DocumentCachedRange()) {
    // Otherwise, it must create a new range, set both its start and end to the
    // start of the context object's range,
    Range* new_range = current_range->cloneRange();
    new_range->collapse(true);

    // and then set the context object's range to the newly-created range.
    SelectionInDOMTree::Builder builder;
    builder.Collapse(new_range->StartPosition());
    UpdateFrameSelection(builder.Build(), new_range, SetSelectionOptions());
  } else {
    // TODO(tkent): The Selection API doesn't define this behavior. We should
    // discuss this on https://github.com/w3c/selection-api/issues/83.
    SelectionInDOMTree::Builder builder;
    builder.Collapse(
        GetFrame()->Selection().GetSelectionInDOMTree().ComputeStartPosition());
    UpdateFrameSelection(builder.Build(), nullptr, SetSelectionOptions());
  }
}

void DOMSelection::empty() {
  if (!IsAvailable())
    return;
  GetFrame()->Selection().Clear();
}

void DOMSelection::setBaseAndExtent(Node* base_node,
                                    unsigned base_offset,
                                    Node* extent_node,
                                    unsigned extent_offset,
                                    ExceptionState& exception_state) {
  if (!IsAvailable())
    return;

  // TODO(editing-dev): Behavior on where base or extent is null is still
  // under discussion: https://github.com/w3c/selection-api/issues/72
  if (!base_node) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      WebFeature::kSelectionSetBaseAndExtentNull);
    GetFrame()->Selection().Clear();
    return;
  }
  if (!extent_node) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      WebFeature::kSelectionSetBaseAndExtentNull);
    extent_offset = 0;
  }

  Range::CheckNodeWOffset(base_node, base_offset, exception_state);
  if (exception_state.HadException())
    return;
  if (extent_node) {
    Range::CheckNodeWOffset(extent_node, extent_offset, exception_state);
    if (exception_state.HadException())
      return;
  }

  if (!IsValidForPosition(base_node) || !IsValidForPosition(extent_node))
    return;

  ClearCachedRangeIfSelectionOfDocument();

  Position base_position(base_node, base_offset);
  Position extent_position(extent_node, extent_offset);
  Range* new_range = Range::Create(base_node->GetDocument());
  if (extent_position.IsNull()) {
    new_range->setStart(base_node, base_offset);
    new_range->setEnd(base_node, base_offset);
  } else if (base_position < extent_position) {
    new_range->setStart(base_node, base_offset);
    new_range->setEnd(extent_node, extent_offset);
  } else {
    new_range->setStart(extent_node, extent_offset);
    new_range->setEnd(base_node, base_offset);
  }
  UpdateFrameSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtentDeprecated(base_position, extent_position)
          .Build(),
      new_range, SetSelectionOptions::Builder().SetIsDirectional(true).Build());
}

void DOMSelection::modify(const String& alter_string,
                          const String& direction_string,
                          const String& granularity_string) {
  if (!IsAvailable())
    return;

  SelectionModifyAlteration alter;
  if (DeprecatedEqualIgnoringCase(alter_string, "extend"))
    alter = SelectionModifyAlteration::kExtend;
  else if (DeprecatedEqualIgnoringCase(alter_string, "move"))
    alter = SelectionModifyAlteration::kMove;
  else
    return;

  SelectionModifyDirection direction;
  if (DeprecatedEqualIgnoringCase(direction_string, "forward"))
    direction = SelectionModifyDirection::kForward;
  else if (DeprecatedEqualIgnoringCase(direction_string, "backward"))
    direction = SelectionModifyDirection::kBackward;
  else if (DeprecatedEqualIgnoringCase(direction_string, "left"))
    direction = SelectionModifyDirection::kLeft;
  else if (DeprecatedEqualIgnoringCase(direction_string, "right"))
    direction = SelectionModifyDirection::kRight;
  else
    return;

  TextGranularity granularity;
  if (DeprecatedEqualIgnoringCase(granularity_string, "character"))
    granularity = TextGranularity::kCharacter;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "word"))
    granularity = TextGranularity::kWord;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "sentence"))
    granularity = TextGranularity::kSentence;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "line"))
    granularity = TextGranularity::kLine;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "paragraph"))
    granularity = TextGranularity::kParagraph;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "lineboundary"))
    granularity = TextGranularity::kLineBoundary;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "sentenceboundary"))
    granularity = TextGranularity::kSentenceBoundary;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "paragraphboundary"))
    granularity = TextGranularity::kParagraphBoundary;
  else if (DeprecatedEqualIgnoringCase(granularity_string, "documentboundary"))
    granularity = TextGranularity::kDocumentBoundary;
  else
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  Element* focused_element = GetFrame()->GetDocument()->FocusedElement();
  GetFrame()->Selection().Modify(alter, direction, granularity,
                                 SetSelectionBy::kSystem);
  if (GetFrame() && GetFrame()->GetDocument() &&
      focused_element != GetFrame()->GetDocument()->FocusedElement()) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      WebFeature::kSelectionFuncionsChangeFocus);
  }
}

// https://www.w3.org/TR/selection-api/#dom-selection-extend
void DOMSelection::extend(Node* node,
                          unsigned offset,
                          ExceptionState& exception_state) {
  DCHECK(node);
  if (!IsAvailable())
    return;

  // 1. If node's root is not the document associated with the context object,
  // abort these steps.
  if (!IsValidForPosition(node))
    return;

  // 2. If the context object is empty, throw an InvalidStateError exception and
  // abort these steps.
  if (rangeCount() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "This Selection object doesn't have any Ranges.");
    return;
  }

  Range::CheckNodeWOffset(node, offset, exception_state);
  if (exception_state.HadException())
    return;

  // 3. Let oldAnchor and oldFocus be the context object's anchor and focus, and
  // let newFocus be the boundary point (node, offset).
  const Position old_anchor(anchorNode(), anchorOffset());
  DCHECK(!old_anchor.IsNull());
  const Position new_focus(node, offset);

  ClearCachedRangeIfSelectionOfDocument();

  // 4. Let newRange be a new range.
  Range* new_range = Range::Create(*GetFrame()->GetDocument());

  // 5. If node's root is not the same as the context object's range's root, set
  // newRange's start and end to newFocus.
  // E.g. oldAnchor might point in shadow Text node in TextControlElement.
  if (old_anchor.AnchorNode()->TreeRoot() != node->TreeRoot()) {
    new_range->setStart(node, offset);
    new_range->setEnd(node, offset);

  } else if (old_anchor <= new_focus) {
    // 6. Otherwise, if oldAnchor is before or equal to newFocus, set newRange's
    // start to oldAnchor, then set its end to newFocus.
    new_range->setStart(old_anchor.AnchorNode(),
                        old_anchor.OffsetInContainerNode());
    new_range->setEnd(node, offset);

  } else {
    // 7. Otherwise, set newRange's start to newFocus, then set its end to
    // oldAnchor.
    new_range->setStart(node, offset);
    new_range->setEnd(old_anchor.AnchorNode(),
                      old_anchor.OffsetInContainerNode());
  }

  // 8. Set the context object's range to newRange.
  SelectionInDOMTree::Builder builder;
  if (new_range->collapsed())
    builder.Collapse(new_focus);
  else
    builder.Collapse(old_anchor).Extend(new_focus);
  UpdateFrameSelection(
      builder.Build(), new_range,
      SetSelectionOptions::Builder().SetIsDirectional(true).Build());
}

Range* DOMSelection::getRangeAt(unsigned index,
                                ExceptionState& exception_state) const {
  if (!IsAvailable())
    return nullptr;

  if (index >= rangeCount()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Number(index) + " is not a valid index.");
    return nullptr;
  }

  // If you're hitting this, you've added broken multi-range selection support
  DCHECK_EQ(rangeCount(), 1u);

  if (Range* cached_range = DocumentCachedRange())
    return cached_range;

  Range* range = CreateRange(CreateRangeFromSelectionEditor());
  CacheRangeIfSelectionOfDocument(range);
  return range;
}

Range* DOMSelection::PrimaryRangeOrNull() const {
  return rangeCount() > 0 ? getRangeAt(0, ASSERT_NO_EXCEPTION) : nullptr;
}

EphemeralRange DOMSelection::CreateRangeFromSelectionEditor() const {
  const VisibleSelection& selection = GetVisibleSelection();
  const Position& anchor = blink::AnchorPosition(selection);
  if (IsSelectionOfDocument() && !anchor.AnchorNode()->IsInShadowTree())
    return FirstEphemeralRangeOf(selection);

  Node* const anchor_node = ShadowAdjustedNode(anchor);
  if (!anchor_node)  // crbug.com/595100
    return EphemeralRange();

  const Position& focus = FocusPosition(selection);
  const Position shadow_adjusted_focus =
      Position(ShadowAdjustedNode(focus), ShadowAdjustedOffset(focus));
  const Position shadow_adjusted_anchor =
      Position(anchor_node, ShadowAdjustedOffset(anchor));
  if (selection.IsBaseFirst())
    return EphemeralRange(shadow_adjusted_anchor, shadow_adjusted_focus);
  return EphemeralRange(shadow_adjusted_focus, shadow_adjusted_anchor);
}

bool DOMSelection::IsSelectionOfDocument() const {
  return tree_scope_ == tree_scope_->GetDocument();
}

void DOMSelection::CacheRangeIfSelectionOfDocument(Range* range) const {
  if (!IsSelectionOfDocument())
    return;
  if (!GetFrame())
    return;
  GetFrame()->Selection().CacheRangeOfDocument(range);
}

Range* DOMSelection::DocumentCachedRange() const {
  if (!IsSelectionOfDocument())
    return nullptr;
  return GetFrame()->Selection().DocumentCachedRange();
}

void DOMSelection::ClearCachedRangeIfSelectionOfDocument() {
  if (!IsSelectionOfDocument())
    return;
  GetFrame()->Selection().ClearDocumentCachedRange();
}

void DOMSelection::removeRange(Range* range) {
  DCHECK(range);
  if (!IsAvailable())
    return;
  if (range == PrimaryRangeOrNull())
    GetFrame()->Selection().Clear();
}

void DOMSelection::removeAllRanges() {
  if (!IsAvailable())
    return;
  GetFrame()->Selection().Clear();
}

void DOMSelection::addRange(Range* new_range) {
  DCHECK(new_range);

  if (!IsAvailable())
    return;

  if (new_range->OwnerDocument() != GetFrame()->GetDocument())
    return;

  if (!new_range->IsConnected()) {
    AddConsoleWarning("addRange(): The given range isn't in document.");
    return;
  }

  FrameSelection& selection = GetFrame()->Selection();

  if (new_range->OwnerDocument() != selection.GetDocument()) {
    // "editing/selection/selection-in-iframe-removed-crash.html" goes here.
    return;
  }

  if (rangeCount() == 0) {
    UpdateFrameSelection(SelectionInDOMTree::Builder()
                             .Collapse(new_range->StartPosition())
                             .Extend(new_range->EndPosition())
                             .Build(),
                         new_range, SetSelectionOptions());
    return;
  }

  Range* original_range = PrimaryRangeOrNull();
  DCHECK(original_range);

  if (original_range->startContainer()->GetTreeScope() !=
      new_range->startContainer()->GetTreeScope()) {
    return;
  }

  if (original_range->compareBoundaryPoints(Range::kStartToEnd, new_range,
                                            ASSERT_NO_EXCEPTION) < 0 ||
      new_range->compareBoundaryPoints(Range::kStartToEnd, original_range,
                                       ASSERT_NO_EXCEPTION) < 0) {
    return;
  }

  // TODO(tkent): "Merge the ranges if they intersect" was removed. We show a
  // warning message for a while, and continue to collect the usage data.
  // <https://code.google.com/p/chromium/issues/detail?id=353069>.
  Deprecation::CountDeprecation(tree_scope_->GetDocument(),
                                WebFeature::kSelectionAddRangeIntersect);
}

// https://www.w3.org/TR/selection-api/#dom-selection-deletefromdocument
void DOMSelection::deleteFromDocument() {
  if (!IsAvailable())
    return;

  // The method must invoke deleteContents() ([DOM4]) on the context object's
  // range if the context object is not empty. Otherwise the method must do
  // nothing.
  if (Range* range = DocumentCachedRange()) {
    range->deleteContents(ASSERT_NO_EXCEPTION);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  // The following code is necessary for
  // editing/selection/deleteFromDocument-crash.html, which assumes
  // deleteFromDocument() for text selection in a TEXTAREA deletes the TEXTAREA
  // value.

  FrameSelection& selection = GetFrame()->Selection();

  if (selection.ComputeVisibleSelectionInDOMTree().IsNone())
    return;

  Range* selected_range =
      CreateRange(selection.ComputeVisibleSelectionInDOMTree()
                      .ToNormalizedEphemeralRange());
  if (!selected_range)
    return;

  // |selectedRange| may point nodes in a different root.
  selected_range->deleteContents(ASSERT_NO_EXCEPTION);
}

bool DOMSelection::containsNode(const Node* n, bool allow_partial) const {
  DCHECK(n);

  if (!IsAvailable())
    return false;

  if (GetFrame()->GetDocument() != n->GetDocument())
    return false;

  unsigned node_index = n->NodeIndex();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |VisibleSelection::toNormalizedEphemeralRange| requires clean layout.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  FrameSelection& selection = GetFrame()->Selection();
  const EphemeralRange selected_range =
      selection.ComputeVisibleSelectionInDOMTreeDeprecated()
          .ToNormalizedEphemeralRange();
  if (selected_range.IsNull())
    return false;

  ContainerNode* parent_node = n->parentNode();
  if (!parent_node)
    return false;

  const Position start_position =
      selected_range.StartPosition().ToOffsetInAnchor();
  const Position end_position = selected_range.EndPosition().ToOffsetInAnchor();
  DummyExceptionStateForTesting exception_state;
  bool node_fully_selected =
      Range::compareBoundaryPoints(
          parent_node, node_index, start_position.ComputeContainerNode(),
          start_position.OffsetInContainerNode(), exception_state) >= 0 &&
      !exception_state.HadException() &&
      Range::compareBoundaryPoints(
          parent_node, node_index + 1, end_position.ComputeContainerNode(),
          end_position.OffsetInContainerNode(), exception_state) <= 0 &&
      !exception_state.HadException();
  if (exception_state.HadException())
    return false;
  if (node_fully_selected)
    return true;

  bool node_fully_unselected =
      (Range::compareBoundaryPoints(
           parent_node, node_index, end_position.ComputeContainerNode(),
           end_position.OffsetInContainerNode(), exception_state) > 0 &&
       !exception_state.HadException()) ||
      (Range::compareBoundaryPoints(
           parent_node, node_index + 1, start_position.ComputeContainerNode(),
           start_position.OffsetInContainerNode(), exception_state) < 0 &&
       !exception_state.HadException());
  DCHECK(!exception_state.HadException());
  if (node_fully_unselected)
    return false;

  return allow_partial || n->IsTextNode();
}

void DOMSelection::selectAllChildren(Node* n, ExceptionState& exception_state) {
  DCHECK(n);

  // This doesn't (and shouldn't) select text node characters.
  setBaseAndExtent(n, 0, n, n->CountChildren(), exception_state);
}

String DOMSelection::toString() {
  if (!IsAvailable())
    return String();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  const EphemeralRange range = GetFrame()
                                   ->Selection()
                                   .ComputeVisibleSelectionInDOMTreeDeprecated()
                                   .ToNormalizedEphemeralRange();
  return PlainText(
      range,
      TextIteratorBehavior::Builder().SetForSelectionToString(true).Build());
}

Node* DOMSelection::ShadowAdjustedNode(const Position& position) const {
  if (position.IsNull())
    return nullptr;

  Node* container_node = position.ComputeContainerNode();
  Node* adjusted_node = tree_scope_->AncestorInThisScope(container_node);

  if (!adjusted_node)
    return nullptr;

  if (container_node == adjusted_node)
    return container_node;

  DCHECK(!adjusted_node->IsShadowRoot()) << adjusted_node;
  return adjusted_node->ParentOrShadowHostNode();
}

unsigned DOMSelection::ShadowAdjustedOffset(const Position& position) const {
  if (position.IsNull())
    return 0;

  Node* container_node = position.ComputeContainerNode();
  Node* adjusted_node = tree_scope_->AncestorInThisScope(container_node);

  if (!adjusted_node)
    return 0;

  if (container_node == adjusted_node)
    return position.ComputeOffsetInContainerNode();

  return adjusted_node->NodeIndex();
}

bool DOMSelection::IsValidForPosition(Node* node) const {
  DCHECK(GetFrame());
  if (!node)
    return true;
  return node->GetDocument() == GetFrame()->GetDocument() &&
         node->isConnected();
}

void DOMSelection::AddConsoleWarning(const String& message) {
  if (tree_scope_) {
    tree_scope_->GetDocument().AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning, message));
  }
}

void DOMSelection::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
