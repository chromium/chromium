/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/insert_paragraph_separator_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/insert_line_break_command.h"
#include "third_party/blink/renderer/core/editing/editing_style.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

// 3.2.5.2.5 Phrasing content
// https://html.spec.whatwg.org/multipage/dom.html#phrasing-content
bool IsPhrasingContent(const Node* node) {
  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, phrasing_content_names,
                      ({
                          html_names::kATag,        html_names::kAbbrTag,
                          html_names::kAreaTag,     html_names::kAudioTag,
                          html_names::kBTag,        html_names::kBdiTag,
                          html_names::kBdoTag,      html_names::kBrTag,
                          html_names::kButtonTag,   html_names::kCanvasTag,
                          html_names::kCiteTag,     html_names::kCodeTag,
                          html_names::kDataTag,     html_names::kDatalistTag,
                          html_names::kDelTag,      html_names::kDfnTag,
                          html_names::kEmTag,       html_names::kEmbedTag,
                          html_names::kITag,        html_names::kIFrameTag,
                          html_names::kImgTag,      html_names::kInputTag,
                          html_names::kInsTag,      html_names::kKbdTag,
                          html_names::kLabelTag,    html_names::kLinkTag,
                          html_names::kMapTag,      html_names::kMarkTag,
                          mathml_names::kMathTag,   html_names::kMetaTag,
                          html_names::kMeterTag,    html_names::kNoscriptTag,
                          html_names::kObjectTag,   html_names::kOutputTag,
                          html_names::kPictureTag,  html_names::kProgressTag,
                          html_names::kQTag,        html_names::kRubyTag,
                          html_names::kSTag,        html_names::kSampTag,
                          html_names::kScriptTag,   html_names::kSelectTag,
                          html_names::kSlotTag,     html_names::kSmallTag,
                          html_names::kSpanTag,     html_names::kStrongTag,
                          html_names::kSubTag,      html_names::kSupTag,
                          svg_names::kSVGTag,       html_names::kTemplateTag,
                          html_names::kTextareaTag, html_names::kTimeTag,
                          html_names::kUTag,        html_names::kVarTag,
                          html_names::kVideoTag,    html_names::kWbrTag,
                      }));
  if (const auto* element = DynamicTo<Element>(node)) {
    return phrasing_content_names.Contains(element->TagQName());
  }
  return false;
}

bool IsEditableRootPhrasingContent(const Position& position) {
  const ContainerNode* editable_root = HighestEditableRoot(position);
  if (!editable_root) {
    return false;
  }
  return EnclosingNodeOfType(FirstPositionInOrBeforeNode(*editable_root),
                             IsPhrasingContent);
}

}  // namespace

// When inserting a new line, we want to avoid nesting empty divs if we can.
// Otherwise, when pasting, it's easy to have each new line be a div deeper than
// the previous. E.g., in the case below, we want to insert at ^ instead of |.
// <div>foo<div>bar</div>|</div>^
static Element* HighestVisuallyEquivalentDivBelowRoot(Element* start_block) {
  Element* cur_block = start_block;
  // We don't want to return a root node (if it happens to be a div, e.g., in a
  // document fragment) because there are no siblings for us to append to.
  while (!cur_block->nextSibling() &&
         IsA<HTMLDivElement>(*cur_block->parentElement()) &&
         cur_block->parentElement()->parentElement()) {
    if (cur_block->parentElement()->hasAttributes())
      break;
    cur_block = cur_block->parentElement();
  }
  return cur_block;
}

static bool InSameBlock(const VisiblePosition& a, const VisiblePosition& b) {
  DCHECK(a.IsValid()) << a;
  DCHECK(b.IsValid()) << b;
  return !a.IsNull() &&
         EnclosingBlock(a.DeepEquivalent().ComputeContainerNode()) ==
             EnclosingBlock(b.DeepEquivalent().ComputeContainerNode());
}

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(
    Document& document,
    bool must_use_default_paragraph_element,
    bool paste_blockquote_into_unquoted_area)
    : CompositeEditCommand(document),
      must_use_default_paragraph_element_(must_use_default_paragraph_element),
      paste_blockquote_into_unquoted_area_(
          paste_blockquote_into_unquoted_area) {}

bool InsertParagraphSeparatorCommand::PreservesTypingStyle() const {
  return true;
}

void InsertParagraphSeparatorCommand::CalculateStyleBeforeInsertion(
    const Position& pos) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  // It is only important to set a style to apply later if we're at the
  // boundaries of a paragraph. Otherwise, content that is moved as part of the
  // work of the command will lend their styles to the new paragraph without any
  // extra work needed.
  VisiblePosition visible_pos = CreateVisiblePosition(pos);
  if (!IsStartOfParagraph(visible_pos) && !IsEndOfParagraph(visible_pos))
    return;

  DCHECK(pos.IsNotNull());
  style_ = MakeGarbageCollected<EditingStyle>(pos);
  style_->MergeTypingStyle(pos.GetDocument());
}

void InsertParagraphSeparatorCommand::ApplyStyleAfterInsertion(
    Element* original_enclosing_block,
    EditingState* editing_state) {
  // Not only do we break out of header tags, but we also do not preserve the
  // typing style, in order to match other browsers.
  if (original_enclosing_block->HasTagName(html_names::kH1Tag) ||
      original_enclosing_block->HasTagName(html_names::kH2Tag) ||
      original_enclosing_block->HasTagName(html_names::kH3Tag) ||
      original_enclosing_block->HasTagName(html_names::kH4Tag) ||
      original_enclosing_block->HasTagName(html_names::kH5Tag)) {
    return;
  }

  if (!style_)
    return;

  style_->PrepareToApplyAt(EndingVisibleSelection().Start());
  if (!style_->IsEmpty())
    ApplyStyle(style_.Get(), editing_state);
}

bool InsertParagraphSeparatorCommand::ShouldUseDefaultParagraphElement(
    Element* enclosing_block) const {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  if (must_use_default_paragraph_element_)
    return true;

  // Assumes that if there was a range selection, it was already deleted.
  if (!IsEndOfBlock(EndingVisibleSelection().VisibleStart()))
    return false;

  return enclosing_block->HasTagName(html_names::kH1Tag) ||
         enclosing_block->HasTagName(html_names::kH2Tag) ||
         enclosing_block->HasTagName(html_names::kH3Tag) ||
         enclosing_block->HasTagName(html_names::kH4Tag) ||
         enclosing_block->HasTagName(html_names::kH5Tag);
}

void InsertParagraphSeparatorCommand::GetAncestorsInsideBlock(
    const Node* insertion_node,
    Element* outer_block,
    HeapVector<Member<Element>>& ancestors) {
  ancestors.clear();

  // Build up list of ancestors elements between the insertion node and the
  // outer block.
  if (insertion_node != outer_block) {
    for (Element* n = insertion_node->parentElement(); n && n != outer_block;
         n = n->parentElement())
      ancestors.push_back(n);
  }
}

Element* InsertParagraphSeparatorCommand::CloneHierarchyUnderNewBlock(
    const HeapVector<Member<Element>>& ancestors,
    Element* block_to_insert,
    EditingState* editing_state) {
  // Make clones of ancestors in between the start node and the start block.
  Element* parent = block_to_insert;
  for (wtf_size_t i = ancestors.size(); i != 0; --i) {
    Element& ancestor = *ancestors[i - 1];
    Element& child = ancestor.CloneWithoutChildren();
    // It should always be okay to remove id from the cloned elements, since the
    // originals are not deleted.
    child.removeAttribute(html_names::kIdAttr);
    AppendNode(&child, parent, editing_state);
    if (editing_state->IsAborted())
      return nullptr;
    parent = &child;
  }

  return parent;
}

void InsertParagraphSeparatorCommand::DoApply(EditingState* editing_state) {
  // TODO(editing-dev): We shouldn't construct an
  // InsertParagraphSeparatorCommand with none or invalid selection.
  const VisibleSelection& visible_selection = EndingVisibleSelection();
  if (visible_selection.IsNone() ||
      !visible_selection.IsValidFor(GetDocument()))
    return;

  Position insertion_position = visible_selection.Start();

  TextAffinity affinity = visible_selection.Affinity();

  // Delete the current selection.
  if (EndingSelection().IsRange()) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    CalculateStyleBeforeInsertion(insertion_position);
    if (!DeleteSelection(editing_state, DeleteSelectionOptions::NormalDelete()))
      return;
    const VisibleSelection& visble_selection_after_delete =
        EndingVisibleSelection();
    insertion_position = visble_selection_after_delete.Start();
    affinity = visble_selection_after_delete.Affinity();
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // FIXME: The parentAnchoredEquivalent conversion needs to be moved into
  // enclosingBlock.
  Element* start_block = EnclosingBlock(
      insertion_position.ParentAnchoredEquivalent().ComputeContainerNode());
  Node* list_child_node = EnclosingListChild(
      insertion_position.ParentAnchoredEquivalent().ComputeContainerNode());
  auto* list_child = DynamicTo<HTMLElement>(list_child_node);
  Position canonical_pos =
      CreateVisiblePosition(insertion_position).DeepEquivalent();
  if (!start_block || !start_block->NonShadowBoundaryParentNode() ||
      (RuntimeEnabledFeatures::InsertLineBreakIfPhrasingContentEnabled() &&
       IsEditableRootPhrasingContent(insertion_position)) ||
      IsTableCell(start_block) ||
      IsA<HTMLFormElement>(*start_block)
      // FIXME: If the node is hidden, we don't have a canonical position so we
      // will do the wrong thing for tables and <hr>.
      // https://bugs.webkit.org/show_bug.cgi?id=40342
      || (!canonical_pos.IsNull() &&
          IsDisplayInsideTable(canonical_pos.AnchorNode())) ||
      (!canonical_pos.IsNull() &&
       IsA<HTMLHRElement>(*canonical_pos.AnchorNode()))) {
    ApplyCommandToComposite(
        MakeGarbageCollected<InsertLineBreakCommand>(GetDocument()),
        editing_state);
    return;
  }

  // Use the leftmost candidate.
  insertion_position = MostBackwardCaretPosition(insertion_position);
  if (!IsVisuallyEquivalentCandidate(insertion_position))
    insertion_position = MostForwardCaretPosition(insertion_position);

  // Adjust the insertion position after the delete
  const Position original_insertion_position = insertion_position;
  const Element* enclosing_anchor =
      EnclosingAnchorElement(original_insertion_position);
  insertion_position =
      PositionAvoidingSpecialElementBoundary(insertion_position, editing_state);
  if (editing_state->IsAborted())
    return;
  // InsertTextCommandTest.AnchorElementWithBlockCrash reaches here.
  ABORT_EDITING_COMMAND_IF(!start_block->parentNode());
  if (list_child == enclosing_anchor) {
    // |positionAvoidingSpecialElementBoundary()| creates new A element and
    // move to another place.
    list_child =
        To<HTMLElement>(EnclosingAnchorElement(original_insertion_position));
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  CalculateStyleBeforeInsertion(insertion_position);

  //---------------------------------------------------------------------
  // Handle special case of typing return on an empty list item
  if (BreakOutOfEmptyListItem(editing_state) || editing_state->IsAborted())
    return;

  //---------------------------------------------------------------------
  // Prepare for more general cases.

  // Create block to be inserted.
  bool nest_new_block = false;
  Element* block_to_insert = nullptr;
  if (IsRootEditableElement(*start_block)) {
    block_to_insert = CreateDefaultParagraphElement(GetDocument());
    nest_new_block = true;
  } else if (ShouldUseDefaultParagraphElement(start_block)) {
    block_to_insert = CreateDefaultParagraphElement(GetDocument());
  } else {
    block_to_insert = &start_block->CloneWithoutChildren();
  }

  VisiblePosition visible_pos =
      CreateVisiblePosition(insertion_position, affinity);
  bool is_first_in_block = IsStartOfBlock(visible_pos);
  bool is_last_in_block = IsEndOfBlock(visible_pos);

  //---------------------------------------------------------------------
  // Handle case when position is in the last visible position in its block,
  // including when the block is empty.
  if (is_last_in_block) {
    if (nest_new_block) {
      if (is_first_in_block && !LineBreakExistsAtVisiblePosition(visible_pos)) {
        // The block is empty.  Create an empty block to
        // represent the paragraph that we're leaving.
        HTMLElement* extra_block = CreateDefaultParagraphElement(GetDocument());
        AppendNode(extra_block, start_block, editing_state);
        if (editing_state->IsAborted())
          return;
        AppendBlockPlaceholder(extra_block, editing_state);
        if (editing_state->IsAborted())
          return;
      }
      AppendNode(block_to_insert, start_block, editing_state);
      if (editing_state->IsAborted())
        return;
    } else {
      // We can get here if we pasted a copied portion of a blockquote with a
      // newline at the end and are trying to paste it into an unquoted area. We
      // then don't want the newline within the blockquote or else it will also
      // be quoted.
      if (paste_blockquote_into_unquoted_area_) {
        if (auto* highest_blockquote =
                To<HTMLQuoteElement>(HighestEnclosingNodeOfType(
                    canonical_pos, &IsMailHTMLBlockquoteElement)))
          start_block = highest_blockquote;
      }

      if (list_child && list_child != start_block) {
        Element& list_child_to_insert = list_child->CloneWithoutChildren();
        AppendNode(block_to_insert, &list_child_to_insert, editing_state);
        if (editing_state->IsAborted())
          return;
        InsertNodeAfter(&list_child_to_insert, list_child, editing_state);
      } else {
        // Most of the time we want to stay at the nesting level of the
        // startBlock (e.g., when nesting within lists). However, for div nodes,
        // this can result in nested div tags that are hard to break out of.
        Element* sibling_element = start_block;
        if (IsA<HTMLDivElement>(*block_to_insert))
          sibling_element = HighestVisuallyEquivalentDivBelowRoot(start_block);
        InsertNodeAfter(block_to_insert, sibling_element, editing_state);
      }
      if (editing_state->IsAborted())
        return;
    }

    // Recreate the same structure in the new paragraph.

    HeapVector<Member<Element>> ancestors;
    GetAncestorsInsideBlock(
        PositionOutsideTabSpan(insertion_position).AnchorNode(), start_block,
        ancestors);
    Element* parent =
        CloneHierarchyUnderNewBlock(ancestors, block_to_insert, editing_state);
    if (editing_state->IsAborted())
      return;

    AppendBlockPlaceholder(parent, editing_state);
    if (editing_state->IsAborted())
      return;

    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::FirstPositionInNode(*parent))
            .Build()));
    return;
  }

  //---------------------------------------------------------------------
  // Handle case when position is in the first visible position in its block,
  // and similar case where previous position is in another, presumeably nested,
  // block.
  if (is_first_in_block ||
      !InSameBlock(visible_pos, PreviousPositionOf(visible_pos))) {
    Node* ref_node = nullptr;
    insertion_position = PositionOutsideTabSpan(insertion_position);

    if (is_first_in_block && !nest_new_block) {
      if (list_child && list_child != start_block) {
        Element& list_child_to_insert = list_child->CloneWithoutChildren();
        AppendNode(block_to_insert, &list_child_to_insert, editing_state);
        if (editing_state->IsAborted())
          return;
        InsertNodeBefore(&list_child_to_insert, list_child, editing_state);
        if (editing_state->IsAborted())
          return;
      } else {
        ref_node = start_block;
      }
    } else if (is_first_in_block && nest_new_block) {
      // startBlock should always have children, otherwise isLastInBlock would
      // be true and it's handled above.
      DCHECK(start_block->HasChildren());
      ref_node = start_block->firstChild();
    } else if (insertion_position.AnchorNode() == start_block &&
               nest_new_block) {
      ref_node = NodeTraversal::ChildAt(
          *start_block, insertion_position.ComputeEditingOffset());
      DCHECK(ref_node);  // must be true or we'd be in the end of block case
    } else {
      ref_node = insertion_position.AnchorNode();
    }

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    // find ending selection position easily before inserting the paragraph
    insertion_position = MostForwardCaretPosition(insertion_position);

    if (ref_node) {
      InsertNodeBefore(block_to_insert, ref_node, editing_state);
      if (editing_state->IsAborted())
        return;
    }

    // Recreate the same structure in the new paragraph.

    HeapVector<Member<Element>> ancestors;
    insertion_position = PositionAvoidingSpecialElementBoundary(
        PositionOutsideTabSpan(insertion_position), editing_state);
    if (editing_state->IsAborted())
      return;
    GetAncestorsInsideBlock(insertion_position.AnchorNode(), start_block,
                            ancestors);

    Element* placeholder =
        CloneHierarchyUnderNewBlock(ancestors, block_to_insert, editing_state);
    if (editing_state->IsAborted())
      return;
    AppendBlockPlaceholder(placeholder, editing_state);
    if (editing_state->IsAborted())
      return;

    // In this case, we need to set the new ending selection.
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(insertion_position)
            .Build()));
    return;
  }

  //---------------------------------------------------------------------
  // Handle the (more complicated) general case,

  // All of the content in the current block after visiblePos is
  // about to be wrapped in a new paragraph element.  Add a br before
  // it if visiblePos is at the start of a paragraph so that the
  // content will move down a line.
  if (IsStartOfParagraph(visible_pos)) {
    auto* br = MakeGarbageCollected<HTMLBRElement>(GetDocument());
    InsertNodeAt(br, insertion_position, editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    insertion_position = Position::InParentAfterNode(*br);
    visible_pos = CreateVisiblePosition(insertion_position);
    // If the insertion point is a break element, there is nothing else
    // we need to do.
    if (visible_pos.IsNotNull() &&
        visible_pos.DeepEquivalent().AnchorNode()->GetLayoutObject()->IsBR()) {
      SetEndingSelection(SelectionForUndoStep::From(
          SelectionInDOMTree::Builder()
              .Collapse(insertion_position)
              .Build()));
      return;
    }
  }

  // Move downstream. Typing style code will take care of carrying along the
  // style of the upstream position.
  insertion_position = MostForwardCaretPosition(insertion_position);

  // At this point, the insertionPosition's node could be a container, and we
  // want to make sure we include all of the correct nodes when building the
  // ancestor list. So this needs to be the deepest representation of the
  // position before we walk the DOM tree.
  VisiblePosition visible_insertion_position =
      CreateVisiblePosition(insertion_position);
  ABORT_EDITING_COMMAND_IF(visible_insertion_position.IsNull());

  insertion_position =
      PositionOutsideTabSpan(visible_insertion_position.DeepEquivalent());
  // If the returned position lies either at the end or at the start of an
  // element that is ignored by editing we should move to its upstream or
  // downstream position.
  if (EditingIgnoresContent(*insertion_position.AnchorNode())) {
    if (insertion_position.AtLastEditingPositionForNode())
      insertion_position = MostForwardCaretPosition(insertion_position);
    else if (insertion_position.AtFirstEditingPositionForNode())
      insertion_position = MostBackwardCaretPosition(insertion_position);
  }

  ABORT_EDITING_COMMAND_IF(!IsEditablePosition(insertion_position));
  // Make sure we do not cause a rendered space to become unrendered.
  // FIXME: We need the affinity for pos, but mostForwardCaretPosition does not
  // give it
  Position leading_whitespace = LeadingCollapsibleWhitespacePosition(
      insertion_position, TextAffinity::kDefault);
  // FIXME: leadingCollapsibleWhitespacePosition is returning the position
  // before preserved newlines for positions after the preserved newline,
  // causing the newline to be turned into a nbsp.
  if (leading_whitespace.IsNotNull()) {
    if (auto* text_node = DynamicTo<Text>(leading_whitespace.AnchorNode())) {
      DCHECK(!text_node->GetLayoutObject() ||
             text_node->GetLayoutObject()->Style()->ShouldCollapseWhiteSpaces())
          << text_node;
      ReplaceTextInNode(text_node,
                        leading_whitespace.ComputeOffsetInContainerNode(), 1,
                        NonBreakingSpaceString());
      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    }
  }

  // Split at pos if in the middle of a text node.
  Position position_after_split;
  if (insertion_position.IsOffsetInAnchor()) {
    if (auto* text_node =
            DynamicTo<Text>(insertion_position.ComputeContainerNode())) {
      int text_offset = insertion_position.OffsetInContainerNode();
      bool at_end = static_cast<unsigned>(text_offset) >= text_node->length();
      if (text_offset > 0 && !at_end) {
        SplitTextNode(text_node, text_offset);
        GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

        position_after_split = Position::FirstPositionInNode(*text_node);
        insertion_position =
            Position(text_node->previousSibling(), text_offset);
      }
    }
  }

  // If we got detached due to mutation events, just bail out.
  if (!start_block->parentNode())
    return;

  // Put the added block in the tree.
  if (nest_new_block) {
    AppendNode(block_to_insert, start_block, editing_state);
  } else if (list_child && list_child != start_block) {
    Element& list_child_to_insert = list_child->CloneWithoutChildren();
    AppendNode(block_to_insert, &list_child_to_insert, editing_state);
    if (editing_state->IsAborted())
      return;
    InsertNodeAfter(&list_child_to_insert, list_child, editing_state);
  } else {
    InsertNodeAfter(block_to_insert, start_block, editing_state);
  }
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  visible_pos = CreateVisiblePosition(insertion_position);

  // If the paragraph separator was inserted at the end of a paragraph, an empty
  // line must be created.  All of the nodes, starting at visiblePos, are about
  // to be added to the new paragraph element.  If the first node to be inserted
  // won't be one that will hold an empty line open, add a br.
  if (IsEndOfParagraph(visible_pos) &&
      !LineBreakExistsAtVisiblePosition(visible_pos)) {
    AppendNode(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
               block_to_insert, editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  }

  // Move the start node and the siblings of the start node.
  if (CreateVisiblePosition(insertion_position).DeepEquivalent() !=
      VisiblePosition::BeforeNode(*block_to_insert).DeepEquivalent()) {
    Node* n;
    if (insertion_position.ComputeContainerNode() == start_block) {
      n = insertion_position.ComputeNodeAfterPosition();
    } else {
      Node* split_to = insertion_position.ComputeContainerNode();
      if (split_to->IsTextNode() &&
          insertion_position.OffsetInContainerNode() >=
              CaretMaxOffset(split_to))
        split_to = NodeTraversal::Next(*split_to, start_block);
      if (split_to)
        SplitTreeToNode(split_to, start_block);

      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

      for (n = start_block->firstChild(); n; n = n->nextSibling()) {
        VisiblePosition before_node_position = VisiblePosition::BeforeNode(*n);
        if (!before_node_position.IsNull() &&
            ComparePositions(CreateVisiblePosition(insertion_position),
                             before_node_position) <= 0)
          break;
      }
    }

    MoveRemainingSiblingsToNewParent(n, block_to_insert, block_to_insert,
                                     editing_state);
    if (editing_state->IsAborted())
      return;
  }

  // Handle whitespace that occurs after the split
  if (position_after_split.IsNotNull()) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    if (!IsRenderedCharacter(position_after_split)) {
      // Clear out all whitespace and insert one non-breaking space
      DCHECK(!position_after_split.ComputeContainerNode()->GetLayoutObject() ||
             position_after_split.ComputeContainerNode()
                 ->GetLayoutObject()
                 ->Style()
                 ->ShouldCollapseWhiteSpaces())
          << position_after_split;
      DeleteInsignificantTextDownstream(position_after_split);
      if (position_after_split.AnchorNode()->IsTextNode()) {
        InsertTextIntoNode(
            To<Text>(position_after_split.ComputeContainerNode()), 0,
            NonBreakingSpaceString());
      }
    }
  }

  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*block_to_insert))
          .Build()));
  ApplyStyleAfterInsertion(start_block, editing_state);
}

void InsertParagraphSeparatorCommand::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
