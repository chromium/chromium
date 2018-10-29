/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/replace_selection_command.h"

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/commands/break_blockquote_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/simplify_markup_command.h"
#include "third_party/blink/renderer/core/editing/commands/smart_replace.h"
#include "third_party/blink/renderer/core/editing/editing_style.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using namespace HTMLNames;

enum EFragmentType { kEmptyFragment, kSingleTextNodeFragment, kTreeFragment };

// --- ReplacementFragment helper class

class ReplacementFragment final {
  STACK_ALLOCATED();

 public:
  ReplacementFragment(Document*, DocumentFragment*, const VisibleSelection&);

  Node* FirstChild() const;
  Node* LastChild() const;

  bool IsEmpty() const;

  bool HasInterchangeNewlineAtStart() const {
    return has_interchange_newline_at_start_;
  }
  bool HasInterchangeNewlineAtEnd() const {
    return has_interchange_newline_at_end_;
  }

  void RemoveNode(Node*);
  void RemoveNodePreservingChildren(ContainerNode*);

 private:
  HTMLElement* InsertFragmentForTestRendering(Element* root_editable_element);
  void RemoveUnrenderedNodes(ContainerNode*);
  void RestoreAndRemoveTestRenderingNodesToFragment(Element*);
  void RemoveInterchangeNodes(ContainerNode*);

  void InsertNodeBefore(Node*, Node* ref_node);

  Member<Document> document_;
  Member<DocumentFragment> fragment_;
  bool has_interchange_newline_at_start_;
  bool has_interchange_newline_at_end_;

  DISALLOW_COPY_AND_ASSIGN(ReplacementFragment);
};

static bool IsInterchangeHTMLBRElement(const Node* node) {
  DEFINE_STATIC_LOCAL(String, interchange_newline_class_string,
                      (AppleInterchangeNewline));
  if (!IsHTMLBRElement(node) ||
      ToHTMLBRElement(node)->getAttribute(classAttr) !=
          interchange_newline_class_string)
    return false;
  UseCounter::Count(node->GetDocument(),
                    WebFeature::kEditingAppleInterchangeNewline);
  return true;
}

static Position PositionAvoidingPrecedingNodes(Position pos) {
  // If we're already on a break, it's probably a placeholder and we shouldn't
  // change our position.
  if (EditingIgnoresContent(*pos.AnchorNode()))
    return pos;

  // We also stop when changing block flow elements because even though the
  // visual position is the same.  E.g.,
  //   <div>foo^</div>^
  // The two positions above are the same visual position, but we want to stay
  // in the same block.
  Element* enclosing_block_element = EnclosingBlock(pos.ComputeContainerNode());
  for (Position next_position = pos;
       next_position.ComputeContainerNode() != enclosing_block_element;
       pos = next_position) {
    if (LineBreakExistsAtPosition(pos))
      break;

    if (pos.ComputeContainerNode()->NonShadowBoundaryParentNode())
      next_position = Position::InParentAfterNode(*pos.ComputeContainerNode());

    if (next_position == pos ||
        EnclosingBlock(next_position.ComputeContainerNode()) !=
            enclosing_block_element ||
        CreateVisiblePosition(pos).DeepEquivalent() !=
            CreateVisiblePosition(next_position).DeepEquivalent())
      break;
  }
  return pos;
}

ReplacementFragment::ReplacementFragment(Document* document,
                                         DocumentFragment* fragment,
                                         const VisibleSelection& selection)
    : document_(document),
      fragment_(fragment),
      has_interchange_newline_at_start_(false),
      has_interchange_newline_at_end_(false) {
  if (!document_)
    return;
  if (!fragment_ || !fragment_->HasChildren())
    return;

  TRACE_EVENT0("blink", "ReplacementFragment constructor");
  Element* editable_root = selection.RootEditableElement();
  DCHECK(editable_root);
  if (!editable_root)
    return;

  document_->UpdateStyleAndLayoutIgnorePendingStylesheets();

  Element* shadow_ancestor_element;
  if (editable_root->IsInShadowTree())
    shadow_ancestor_element = editable_root->OwnerShadowHost();
  else
    shadow_ancestor_element = editable_root;

  if (!editable_root->GetAttributeEventListener(
          EventTypeNames::webkitBeforeTextInserted)
      // FIXME: Remove these checks once textareas and textfields actually
      // register an event handler.
      &&
      !(shadow_ancestor_element && shadow_ancestor_element->GetLayoutObject() &&
        shadow_ancestor_element->GetLayoutObject()->IsTextControl()) &&
      HasRichlyEditableStyle(*editable_root)) {
    RemoveInterchangeNodes(fragment_.Get());
    return;
  }

  if (!HasRichlyEditableStyle(*editable_root)) {
    bool is_plain_text = true;
    for (Node& node : NodeTraversal::ChildrenOf(*fragment_)) {
      if (IsInterchangeHTMLBRElement(&node) && &node == fragment_->lastChild())
        continue;
      if (!node.IsTextNode()) {
        is_plain_text = false;
        break;
      }
    }
    // We don't need TestRendering for plain-text editing + plain-text
    // insertion.
    if (is_plain_text) {
      RemoveInterchangeNodes(fragment_.Get());
      String original_text = fragment_->textContent();
      BeforeTextInsertedEvent* event =
          BeforeTextInsertedEvent::Create(original_text);
      editable_root->DispatchEvent(*event);
      if (original_text != event->GetText()) {
        fragment_ = CreateFragmentFromText(
            selection.ToNormalizedEphemeralRange(), event->GetText());
        RemoveInterchangeNodes(fragment_.Get());
      }
      return;
    }
  }

  HTMLElement* holder = InsertFragmentForTestRendering(editable_root);
  if (!holder) {
    RemoveInterchangeNodes(fragment_.Get());
    return;
  }

  const EphemeralRange range =
      CreateVisibleSelection(
          SelectionInDOMTree::Builder().SelectAllChildren(*holder).Build())
          .ToNormalizedEphemeralRange();
  const TextIteratorBehavior& behavior = TextIteratorBehavior::Builder()
                                             .SetEmitsOriginalText(true)
                                             .SetIgnoresStyleVisibility(true)
                                             .Build();
  const String& text = PlainText(range, behavior);

  RemoveInterchangeNodes(holder);
  RemoveUnrenderedNodes(holder);
  RestoreAndRemoveTestRenderingNodesToFragment(holder);

  // Give the root a chance to change the text.
  BeforeTextInsertedEvent* evt = BeforeTextInsertedEvent::Create(text);
  editable_root->DispatchEvent(*evt);
  if (text != evt->GetText() || !HasRichlyEditableStyle(*editable_root)) {
    RestoreAndRemoveTestRenderingNodesToFragment(holder);

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    document->UpdateStyleAndLayoutIgnorePendingStylesheets();

    fragment_ = CreateFragmentFromText(selection.ToNormalizedEphemeralRange(),
                                       evt->GetText());
    if (!fragment_->HasChildren())
      return;

    holder = InsertFragmentForTestRendering(editable_root);
    RemoveInterchangeNodes(holder);
    RemoveUnrenderedNodes(holder);
    RestoreAndRemoveTestRenderingNodesToFragment(holder);
  }
}

bool ReplacementFragment::IsEmpty() const {
  return (!fragment_ || !fragment_->HasChildren()) &&
         !has_interchange_newline_at_start_ && !has_interchange_newline_at_end_;
}

Node* ReplacementFragment::FirstChild() const {
  return fragment_ ? fragment_->firstChild() : nullptr;
}

Node* ReplacementFragment::LastChild() const {
  return fragment_ ? fragment_->lastChild() : nullptr;
}

void ReplacementFragment::RemoveNodePreservingChildren(ContainerNode* node) {
  if (!node)
    return;

  while (Node* n = node->firstChild()) {
    RemoveNode(n);
    InsertNodeBefore(n, node);
  }
  RemoveNode(node);
}

void ReplacementFragment::RemoveNode(Node* node) {
  if (!node)
    return;

  ContainerNode* parent = node->NonShadowBoundaryParentNode();
  if (!parent)
    return;

  parent->RemoveChild(node);
}

void ReplacementFragment::InsertNodeBefore(Node* node, Node* ref_node) {
  if (!node || !ref_node)
    return;

  ContainerNode* parent = ref_node->NonShadowBoundaryParentNode();
  if (!parent)
    return;

  parent->InsertBefore(node, ref_node);
}

HTMLElement* ReplacementFragment::InsertFragmentForTestRendering(
    Element* root_editable_element) {
  TRACE_EVENT0("blink", "ReplacementFragment::insertFragmentForTestRendering");
  DCHECK(document_);
  HTMLElement* holder = CreateDefaultParagraphElement(*document_.Get());

  holder->AppendChild(fragment_);
  root_editable_element->AppendChild(holder);

  // TODO(editing-dev): Hoist this call to the call sites.
  document_->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return holder;
}

void ReplacementFragment::RestoreAndRemoveTestRenderingNodesToFragment(
    Element* holder) {
  if (!holder)
    return;

  while (Node* node = holder->firstChild()) {
    holder->RemoveChild(node);
    fragment_->AppendChild(node);
  }

  RemoveNode(holder);
}

void ReplacementFragment::RemoveUnrenderedNodes(ContainerNode* holder) {
  HeapVector<Member<Node>> unrendered;

  for (Node& node : NodeTraversal::DescendantsOf(*holder)) {
    if (!IsNodeRendered(node) && !IsTableStructureNode(&node))
      unrendered.push_back(&node);
  }

  for (auto& node : unrendered)
    RemoveNode(node);
}

void ReplacementFragment::RemoveInterchangeNodes(ContainerNode* container) {
  has_interchange_newline_at_start_ = false;
  has_interchange_newline_at_end_ = false;

  // Interchange newlines at the "start" of the incoming fragment must be
  // either the first node in the fragment or the first leaf in the fragment.
  Node* node = container->firstChild();
  while (node) {
    if (IsInterchangeHTMLBRElement(node)) {
      has_interchange_newline_at_start_ = true;
      RemoveNode(node);
      break;
    }
    node = node->firstChild();
  }
  if (!container->HasChildren())
    return;
  // Interchange newlines at the "end" of the incoming fragment must be
  // either the last node in the fragment or the last leaf in the fragment.
  node = container->lastChild();
  while (node) {
    if (IsInterchangeHTMLBRElement(node)) {
      has_interchange_newline_at_end_ = true;
      RemoveNode(node);
      break;
    }
    node = node->lastChild();
  }
}

inline void ReplaceSelectionCommand::InsertedNodes::RespondToNodeInsertion(
    Node& node) {
  if (!first_node_inserted_)
    first_node_inserted_ = &node;

  last_node_inserted_ = &node;
}

inline void
ReplaceSelectionCommand::InsertedNodes::WillRemoveNodePreservingChildren(
    Node& node) {
  if (first_node_inserted_.Get() == node)
    first_node_inserted_ = NodeTraversal::Next(node);
  if (last_node_inserted_.Get() == node)
    last_node_inserted_ = node.lastChild()
                              ? node.lastChild()
                              : NodeTraversal::NextSkippingChildren(node);
  if (ref_node_.Get() == node)
    ref_node_ = NodeTraversal::Next(node);
}

inline void ReplaceSelectionCommand::InsertedNodes::WillRemoveNode(Node& node) {
  if (first_node_inserted_.Get() == node && last_node_inserted_.Get() == node) {
    first_node_inserted_ = nullptr;
    last_node_inserted_ = nullptr;
  } else if (first_node_inserted_.Get() == node) {
    first_node_inserted_ =
        NodeTraversal::NextSkippingChildren(*first_node_inserted_);
  } else if (last_node_inserted_.Get() == node) {
    last_node_inserted_ =
        NodeTraversal::PreviousSkippingChildren(*last_node_inserted_);
  }
  if (node.contains(ref_node_))
    ref_node_ = NodeTraversal::NextSkippingChildren(node);
}

inline void ReplaceSelectionCommand::InsertedNodes::DidReplaceNode(
    Node& node,
    Node& new_node) {
  if (first_node_inserted_.Get() == node)
    first_node_inserted_ = &new_node;
  if (last_node_inserted_.Get() == node)
    last_node_inserted_ = &new_node;
  if (ref_node_.Get() == node)
    ref_node_ = &new_node;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(
    Document& document,
    DocumentFragment* fragment,
    CommandOptions options,
    InputEvent::InputType input_type)
    : CompositeEditCommand(document),
      select_replacement_(options & kSelectReplacement),
      smart_replace_(options & kSmartReplace),
      match_style_(options & kMatchStyle),
      document_fragment_(fragment),
      prevent_nesting_(options & kPreventNesting),
      moving_paragraph_(options & kMovingParagraph),
      input_type_(input_type),
      sanitize_fragment_(options & kSanitizeFragment),
      should_merge_end_(false) {}

static bool HasMatchingQuoteLevel(VisiblePosition end_of_existing_content,
                                  VisiblePosition end_of_inserted_content) {
  Position existing = end_of_existing_content.DeepEquivalent();
  Position inserted = end_of_inserted_content.DeepEquivalent();
  bool is_inside_mail_blockquote = EnclosingNodeOfType(
      inserted, IsMailHTMLBlockquoteElement, kCanCrossEditingBoundary);
  return is_inside_mail_blockquote && (NumEnclosingMailBlockquotes(existing) ==
                                       NumEnclosingMailBlockquotes(inserted));
}

bool ReplaceSelectionCommand::ShouldMergeStart(
    bool selection_start_was_start_of_paragraph,
    bool fragment_has_interchange_newline_at_start,
    bool selection_start_was_inside_mail_blockquote) {
  if (moving_paragraph_)
    return false;

  VisiblePosition start_of_inserted_content =
      PositionAtStartOfInsertedContent();
  VisiblePosition prev = PreviousPositionOf(start_of_inserted_content,
                                            kCannotCrossEditingBoundary);
  if (prev.IsNull())
    return false;

  // When we have matching quote levels, its ok to merge more frequently.
  // For a successful merge, we still need to make sure that the inserted
  // content starts with the beginning of a paragraph. And we should only merge
  // here if the selection start was inside a mail blockquote. This prevents
  // against removing a blockquote from newly pasted quoted content that was
  // pasted into an unquoted position. If that unquoted position happens to be
  // right after another blockquote, we don't want to merge and risk stripping a
  // valid block (and newline) from the pasted content.
  if (IsStartOfParagraph(start_of_inserted_content) &&
      selection_start_was_inside_mail_blockquote &&
      HasMatchingQuoteLevel(prev, PositionAtEndOfInsertedContent()))
    return true;

  return !selection_start_was_start_of_paragraph &&
         !fragment_has_interchange_newline_at_start &&
         IsStartOfParagraph(start_of_inserted_content) &&
         !IsHTMLBRElement(
             *start_of_inserted_content.DeepEquivalent().AnchorNode()) &&
         ShouldMerge(start_of_inserted_content, prev);
}

bool ReplaceSelectionCommand::ShouldMergeEnd(
    bool selection_end_was_end_of_paragraph) {
  VisiblePosition end_of_inserted_content(PositionAtEndOfInsertedContent());
  VisiblePosition next =
      NextPositionOf(end_of_inserted_content, kCannotCrossEditingBoundary);
  if (next.IsNull())
    return false;

  return !selection_end_was_end_of_paragraph &&
         IsEndOfParagraph(end_of_inserted_content) &&
         !IsHTMLBRElement(
             *end_of_inserted_content.DeepEquivalent().AnchorNode()) &&
         ShouldMerge(end_of_inserted_content, next);
}

static bool IsHTMLHeaderElement(const Node* a) {
  if (!a || !a->IsHTMLElement())
    return false;

  const HTMLElement& element = ToHTMLElement(*a);
  return element.HasTagName(h1Tag) || element.HasTagName(h2Tag) ||
         element.HasTagName(h3Tag) || element.HasTagName(h4Tag) ||
         element.HasTagName(h5Tag) || element.HasTagName(h6Tag);
}

static bool HaveSameTagName(Element* a, Element* b) {
  return a && b && a->tagName() == b->tagName();
}

bool ReplaceSelectionCommand::ShouldMerge(const VisiblePosition& source,
                                          const VisiblePosition& destination) {
  if (source.IsNull() || destination.IsNull())
    return false;

  Node* source_node = source.DeepEquivalent().AnchorNode();
  Node* destination_node = destination.DeepEquivalent().AnchorNode();
  Element* source_block = EnclosingBlock(source_node);
  Element* destination_block = EnclosingBlock(destination_node);
  return source_block &&
         (!source_block->HasTagName(blockquoteTag) ||
          IsMailHTMLBlockquoteElement(source_block)) &&
         EnclosingListChild(source_block) ==
             EnclosingListChild(destination_node) &&
         EnclosingTableCell(source.DeepEquivalent()) ==
             EnclosingTableCell(destination.DeepEquivalent()) &&
         (!IsHTMLHeaderElement(source_block) ||
          HaveSameTagName(source_block, destination_block))
         // Don't merge to or from a position before or after a block because it
         // would be a no-op and cause infinite recursion.
         && !IsEnclosingBlock(source_node) &&
         !IsEnclosingBlock(destination_node);
}

// Style rules that match just inserted elements could change their appearance,
// like a div inserted into a document with div { display:inline; }.
void ReplaceSelectionCommand::RemoveRedundantStylesAndKeepStyleSpanInline(
    InsertedNodes& inserted_nodes,
    EditingState* editing_state) {
  Node* past_end_node = inserted_nodes.PastLastLeaf();
  Node* next = nullptr;
  for (Node* node = inserted_nodes.FirstNodeInserted();
       node && node != past_end_node; node = next) {
    // FIXME: <rdar://problem/5371536> Style rules that match pasted content can
    // change it's appearance

    next = NodeTraversal::Next(*node);
    if (!node->IsStyledElement())
      continue;

    Element* element = ToElement(node);

    const CSSPropertyValueSet* inline_style = element->InlineStyle();
    EditingStyle* new_inline_style = EditingStyle::Create(inline_style);
    if (inline_style) {
      if (element->IsHTMLElement()) {
        Vector<QualifiedName> attributes;
        HTMLElement* html_element = ToHTMLElement(element);
        DCHECK(html_element);

        if (new_inline_style->ConflictsWithImplicitStyleOfElement(
                html_element)) {
          // e.g. <b style="font-weight: normal;"> is converted to <span
          // style="font-weight: normal;">
          element = ReplaceElementWithSpanPreservingChildrenAndAttributes(
              html_element);
          inline_style = element->InlineStyle();
          inserted_nodes.DidReplaceNode(*html_element, *element);
        } else if (new_inline_style
                       ->ExtractConflictingImplicitStyleOfAttributes(
                           html_element,
                           EditingStyle::kPreserveWritingDirection, nullptr,
                           attributes,
                           EditingStyle::kDoNotExtractMatchingStyle)) {
          // e.g. <font size="3" style="font-size: 20px;"> is converted to <font
          // style="font-size: 20px;">
          for (wtf_size_t i = 0; i < attributes.size(); i++)
            RemoveElementAttribute(html_element, attributes[i]);
        }
      }

      ContainerNode* context = element->parentNode();

      // If Mail wraps the fragment with a Paste as Quotation blockquote, or if
      // you're pasting into a quoted region, styles from blockquoteNode are
      // allowed to override those from the source document, see
      // <rdar://problem/4930986> and <rdar://problem/5089327>.
      HTMLQuoteElement* blockquote_element =
          !context
              ? ToHTMLQuoteElement(context)
              : ToHTMLQuoteElement(EnclosingNodeOfType(
                    Position::FirstPositionInNode(*context),
                    IsMailHTMLBlockquoteElement, kCanCrossEditingBoundary));

      // EditingStyle::removeStyleFromRulesAndContext() uses StyleResolver,
      // which requires clean style.
      // TODO(editing-dev): There is currently no way to update style without
      // updating layout. We might want to have updateLifcycleToStyleClean()
      // similar to FrameView::updateLifecylceToLayoutClean() in Document.
      GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

      if (blockquote_element)
        new_inline_style->RemoveStyleFromRulesAndContext(
            element, GetDocument().documentElement());

      new_inline_style->RemoveStyleFromRulesAndContext(element, context);
    }

    if (!inline_style || new_inline_style->IsEmpty()) {
      if (IsStyleSpanOrSpanWithOnlyStyleAttribute(element) ||
          IsEmptyFontTag(element, kAllowNonEmptyStyleAttribute)) {
        inserted_nodes.WillRemoveNodePreservingChildren(*element);
        RemoveNodePreservingChildren(element, editing_state);
        if (editing_state->IsAborted())
          return;
        continue;
      }
      RemoveElementAttribute(element, styleAttr);
    } else if (new_inline_style->Style()->PropertyCount() !=
               inline_style->PropertyCount()) {
      SetNodeAttribute(element, styleAttr,
                       AtomicString(new_inline_style->Style()->AsText()));
    }

    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    // FIXME: Tolerate differences in id, class, and style attributes.
    if (element->parentNode() && IsNonTableCellHTMLBlockElement(element) &&
        AreIdenticalElements(*element, *element->parentNode()) &&
        VisiblePosition::FirstPositionInNode(*element->parentNode())
                .DeepEquivalent() ==
            VisiblePosition::FirstPositionInNode(*element).DeepEquivalent() &&
        VisiblePosition::LastPositionInNode(*element->parentNode())
                .DeepEquivalent() ==
            VisiblePosition::LastPositionInNode(*element).DeepEquivalent()) {
      inserted_nodes.WillRemoveNodePreservingChildren(*element);
      RemoveNodePreservingChildren(element, editing_state);
      if (editing_state->IsAborted())
        return;
      continue;
    }

    if (element->parentNode() &&
        HasRichlyEditableStyle(*element->parentNode()) &&
        HasRichlyEditableStyle(*element)) {
      RemoveElementAttribute(element, contenteditableAttr);
    }
  }
}

static bool IsProhibitedParagraphChild(const AtomicString& name) {
  // https://dvcs.w3.org/hg/editing/raw-file/57abe6d3cb60/editing.html#prohibited-paragraph-child
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, elements,
                      ({
                          addressTag.LocalName(),   articleTag.LocalName(),
                          asideTag.LocalName(),     blockquoteTag.LocalName(),
                          captionTag.LocalName(),   centerTag.LocalName(),
                          colTag.LocalName(),       colgroupTag.LocalName(),
                          ddTag.LocalName(),        detailsTag.LocalName(),
                          dirTag.LocalName(),       divTag.LocalName(),
                          dlTag.LocalName(),        dtTag.LocalName(),
                          fieldsetTag.LocalName(),  figcaptionTag.LocalName(),
                          figureTag.LocalName(),    footerTag.LocalName(),
                          formTag.LocalName(),      h1Tag.LocalName(),
                          h2Tag.LocalName(),        h3Tag.LocalName(),
                          h4Tag.LocalName(),        h5Tag.LocalName(),
                          h6Tag.LocalName(),        headerTag.LocalName(),
                          hgroupTag.LocalName(),    hrTag.LocalName(),
                          liTag.LocalName(),        listingTag.LocalName(),
                          mainTag.LocalName(),  // Missing in the specification.
                          menuTag.LocalName(),      navTag.LocalName(),
                          olTag.LocalName(),        pTag.LocalName(),
                          plaintextTag.LocalName(), preTag.LocalName(),
                          sectionTag.LocalName(),   summaryTag.LocalName(),
                          tableTag.LocalName(),     tbodyTag.LocalName(),
                          tdTag.LocalName(),        tfootTag.LocalName(),
                          thTag.LocalName(),        theadTag.LocalName(),
                          trTag.LocalName(),        ulTag.LocalName(),
                          xmpTag.LocalName(),
                      }));
  return elements.Contains(name);
}

void ReplaceSelectionCommand::
    MakeInsertedContentRoundTrippableWithHTMLTreeBuilder(
        const InsertedNodes& inserted_nodes,
        EditingState* editing_state) {
  Node* past_end_node = inserted_nodes.PastLastLeaf();
  Node* next = nullptr;
  for (Node* node = inserted_nodes.FirstNodeInserted();
       node && node != past_end_node; node = next) {
    next = NodeTraversal::Next(*node);

    if (!node->IsHTMLElement())
      continue;
    // moveElementOutOfAncestor() in a previous iteration might have failed,
    // and |node| might have been detached from the document tree.
    if (!node->isConnected())
      continue;

    HTMLElement& element = ToHTMLElement(*node);
    if (IsProhibitedParagraphChild(element.localName())) {
      if (HTMLElement* paragraph_element =
              ToHTMLElement(EnclosingElementWithTag(
                  Position::InParentBeforeNode(element), pTag))) {
        MoveElementOutOfAncestor(&element, paragraph_element, editing_state);
        if (editing_state->IsAborted())
          return;
      }
    }

    if (IsHTMLHeaderElement(&element)) {
      if (HTMLElement* header_element = ToHTMLElement(
              HighestEnclosingNodeOfType(Position::InParentBeforeNode(element),
                                         IsHTMLHeaderElement))) {
        MoveElementOutOfAncestor(&element, header_element, editing_state);
        if (editing_state->IsAborted())
          return;
      }
    }
  }
}

void ReplaceSelectionCommand::MoveElementOutOfAncestor(
    Element* element,
    Element* ancestor,
    EditingState* editing_state) {
  DCHECK(element);
  if (!HasEditableStyle(*ancestor->parentNode()))
    return;

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  VisiblePosition position_at_end_of_node =
      CreateVisiblePosition(LastPositionInOrAfterNode(*element));
  VisiblePosition last_position_in_paragraph =
      VisiblePosition::LastPositionInNode(*ancestor);
  if (position_at_end_of_node.DeepEquivalent() ==
      last_position_in_paragraph.DeepEquivalent()) {
    RemoveNode(element, editing_state);
    if (editing_state->IsAborted())
      return;
    if (ancestor->nextSibling())
      InsertNodeBefore(element, ancestor->nextSibling(), editing_state);
    else
      AppendNode(element, ancestor->parentNode(), editing_state);
    if (editing_state->IsAborted())
      return;
  } else {
    Node* node_to_split_to = SplitTreeToNode(element, ancestor, true);
    RemoveNode(element, editing_state);
    if (editing_state->IsAborted())
      return;
    InsertNodeBefore(element, node_to_split_to, editing_state);
    if (editing_state->IsAborted())
      return;
  }
  if (!ancestor->HasChildren())
    RemoveNode(ancestor, editing_state);
}

static inline bool NodeHasVisibleLayoutText(Text& text) {
  return text.GetLayoutObject() &&
         text.GetLayoutObject()->ResolvedTextLength() > 0;
}

void ReplaceSelectionCommand::RemoveUnrenderedTextNodesAtEnds(
    InsertedNodes& inserted_nodes) {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  Node* last_leaf_inserted = inserted_nodes.LastLeafInserted();
  if (last_leaf_inserted && last_leaf_inserted->IsTextNode() &&
      !NodeHasVisibleLayoutText(ToText(*last_leaf_inserted)) &&
      !EnclosingElementWithTag(FirstPositionInOrBeforeNode(*last_leaf_inserted),
                               selectTag) &&
      !EnclosingElementWithTag(FirstPositionInOrBeforeNode(*last_leaf_inserted),
                               scriptTag)) {
    inserted_nodes.WillRemoveNode(*last_leaf_inserted);
    // Removing a Text node won't dispatch synchronous events.
    RemoveNode(last_leaf_inserted, ASSERT_NO_EDITING_ABORT);
  }

  // We don't have to make sure that firstNodeInserted isn't inside a select or
  // script element, because it is a top level node in the fragment and the user
  // can't insert into those elements.
  Node* first_node_inserted = inserted_nodes.FirstNodeInserted();
  if (first_node_inserted && first_node_inserted->IsTextNode() &&
      !NodeHasVisibleLayoutText(ToText(*first_node_inserted))) {
    inserted_nodes.WillRemoveNode(*first_node_inserted);
    // Removing a Text node won't dispatch synchronous events.
    RemoveNode(first_node_inserted, ASSERT_NO_EDITING_ABORT);
  }
}

VisiblePosition ReplaceSelectionCommand::PositionAtEndOfInsertedContent()
    const {
  // TODO(editing-dev): Hoist the call and change it into a DCHECK.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  // TODO(yosin): We should set |end_of_inserted_content_| not in SELECT
  // element, since contents of SELECT elements, e.g. OPTION, OPTGROUP, are
  // not editable, or SELECT element is an atomic on editing.
  HTMLSelectElement* enclosing_select = ToHTMLSelectElement(
      EnclosingElementWithTag(end_of_inserted_content_, selectTag));
  if (enclosing_select) {
    return CreateVisiblePosition(LastPositionInOrAfterNode(*enclosing_select));
  }
  if (end_of_inserted_content_.IsOrphan())
    return VisiblePosition();
  return CreateVisiblePosition(end_of_inserted_content_);
}

VisiblePosition ReplaceSelectionCommand::PositionAtStartOfInsertedContent()
    const {
  // TODO(editing-dev): Hoist the call and change it into a DCHECK.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (start_of_inserted_content_.IsOrphan())
    return VisiblePosition();
  return CreateVisiblePosition(start_of_inserted_content_);
}

static void RemoveHeadContents(ReplacementFragment& fragment) {
  Node* next = nullptr;
  for (Node* node = fragment.FirstChild(); node; node = next) {
    if (IsHTMLBaseElement(*node) || IsHTMLLinkElement(*node) ||
        IsHTMLMetaElement(*node) || IsHTMLTitleElement(*node)) {
      next = NodeTraversal::NextSkippingChildren(*node);
      fragment.RemoveNode(node);
    } else {
      next = NodeTraversal::Next(*node);
    }
  }
}

static bool FollowBlockElementStyle(const Node* node) {
  if (!node->IsHTMLElement())
    return false;

  const HTMLElement& element = ToHTMLElement(*node);
  return IsListItem(node) || IsTableCell(node) || element.HasTagName(preTag) ||
         element.HasTagName(h1Tag) || element.HasTagName(h2Tag) ||
         element.HasTagName(h3Tag) || element.HasTagName(h4Tag) ||
         element.HasTagName(h5Tag) || element.HasTagName(h6Tag);
}

// Remove style spans before insertion if they are unnecessary.  It's faster
// because we'll avoid doing a layout.
static void HandleStyleSpansBeforeInsertion(ReplacementFragment& fragment,
                                            const Position& insertion_pos) {
  Node* top_node = fragment.FirstChild();
  if (!IsHTMLSpanElement(top_node))
    return;

  // Handling the case where we are doing Paste as Quotation or pasting into
  // quoted content is more complicated (see handleStyleSpans) and doesn't
  // receive the optimization.
  if (EnclosingNodeOfType(FirstPositionInOrBeforeNode(*top_node),
                          IsMailHTMLBlockquoteElement,
                          kCanCrossEditingBoundary))
    return;

  // Remove style spans to follow the styles of parent block element when
  // |fragment| becomes a part of it. See bugs http://crbug.com/226941 and
  // http://crbug.com/335955.
  HTMLSpanElement* wrapping_style_span = ToHTMLSpanElement(top_node);
  const Node* node = insertion_pos.AnchorNode();
  // |node| can be an inline element like <br> under <li>
  // e.g.) editing/execCommand/switch-list-type.html
  //       editing/deleting/backspace-merge-into-block.html
  if (IsInline(node)) {
    node = EnclosingBlock(insertion_pos.AnchorNode());
    if (!node)
      return;
  }

  if (FollowBlockElementStyle(node)) {
    fragment.RemoveNodePreservingChildren(wrapping_style_span);
    return;
  }

  EditingStyle* style_at_insertion_pos =
      EditingStyle::Create(insertion_pos.ParentAnchoredEquivalent());
  String style_text = style_at_insertion_pos->Style()->AsText();

  // FIXME: This string comparison is a naive way of comparing two styles.
  // We should be taking the diff and check that the diff is empty.
  if (style_text != wrapping_style_span->getAttribute(styleAttr))
    return;

  fragment.RemoveNodePreservingChildren(wrapping_style_span);
}

void ReplaceSelectionCommand::MergeEndIfNeeded(EditingState* editing_state) {
  if (!should_merge_end_)
    return;

  VisiblePosition start_of_inserted_content(PositionAtStartOfInsertedContent());
  VisiblePosition end_of_inserted_content(PositionAtEndOfInsertedContent());

  // Bail to avoid infinite recursion.
  if (moving_paragraph_) {
    NOTREACHED();
    return;
  }

  // Merging two paragraphs will destroy the moved one's block styles.  Always
  // move the end of inserted forward to preserve the block style of the
  // paragraph already in the document, unless the paragraph to move would
  // include the what was the start of the selection that was pasted into, so
  // that we preserve that paragraph's block styles.
  bool merge_forward =
      !(InSameParagraph(start_of_inserted_content, end_of_inserted_content) &&
        !IsStartOfParagraph(start_of_inserted_content));

  VisiblePosition destination = merge_forward
                                    ? NextPositionOf(end_of_inserted_content)
                                    : end_of_inserted_content;
  // TODO(editing-dev): Stop storing VisiblePositions through mutations.
  // See crbug.com/648949 for details.
  VisiblePosition start_of_paragraph_to_move =
      merge_forward ? StartOfParagraph(end_of_inserted_content)
                    : NextPositionOf(end_of_inserted_content);

  // Merging forward could result in deleting the destination anchor node.
  // To avoid this, we add a placeholder node before the start of the paragraph.
  if (EndOfParagraph(start_of_paragraph_to_move).DeepEquivalent() ==
      destination.DeepEquivalent()) {
    HTMLBRElement* placeholder = HTMLBRElement::Create(GetDocument());
    InsertNodeBefore(placeholder,
                     start_of_paragraph_to_move.DeepEquivalent().AnchorNode(),
                     editing_state);
    if (editing_state->IsAborted())
      return;

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets()
    // needs to be audited.  See http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    destination = VisiblePosition::BeforeNode(*placeholder);
    start_of_paragraph_to_move = CreateVisiblePosition(
        start_of_paragraph_to_move.ToPositionWithAffinity());
  }

  MoveParagraph(start_of_paragraph_to_move,
                EndOfParagraph(start_of_paragraph_to_move), destination,
                editing_state);
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Merging forward will remove end_of_inserted_content from the document.
  if (merge_forward) {
    const VisibleSelection& visible_selection = EndingVisibleSelection();
    if (start_of_inserted_content_.IsOrphan()) {
      start_of_inserted_content_ =
          visible_selection.VisibleStart().DeepEquivalent();
    }
    end_of_inserted_content_ = visible_selection.VisibleEnd().DeepEquivalent();
    // If we merged text nodes, end_of_inserted_content_ could be null. If
    // this is the case, we use start_of_inserted_content_.
    if (end_of_inserted_content_.IsNull())
      end_of_inserted_content_ = start_of_inserted_content_;
  }
}

static Node* EnclosingInline(Node* node) {
  while (ContainerNode* parent = node->parentNode()) {
    if (IsBlockFlowElement(*parent) || IsHTMLBodyElement(*parent))
      return node;
    // Stop if any previous sibling is a block.
    for (Node* sibling = node->previousSibling(); sibling;
         sibling = sibling->previousSibling()) {
      if (IsBlockFlowElement(*sibling))
        return node;
    }
    node = parent;
  }
  return node;
}

static bool IsInlineHTMLElementWithStyle(const Node* node) {
  // We don't want to skip over any block elements.
  if (IsEnclosingBlock(node))
    return false;

  if (!node->IsHTMLElement())
    return false;

  // We can skip over elements whose class attribute is
  // one of our internal classes.
  const HTMLElement* element = ToHTMLElement(node);
  return EditingStyle::ElementIsStyledSpanOrHTMLEquivalent(element);
}

static inline HTMLElement*
ElementToSplitToAvoidPastingIntoInlineElementsWithStyle(
    const Position& insertion_pos) {
  Element* containing_block =
      EnclosingBlock(insertion_pos.ComputeContainerNode());
  return ToHTMLElement(HighestEnclosingNodeOfType(
      insertion_pos, IsInlineHTMLElementWithStyle, kCannotCrossEditingBoundary,
      containing_block));
}

void ReplaceSelectionCommand::SetUpStyle(const VisibleSelection& selection) {
  // We can skip matching the style if the selection is plain text.
  // TODO(editing-dev): Use IsEditablePosition instead of using UserModify
  // directly.
  if ((selection.Start().AnchorNode()->GetLayoutObject() &&
       selection.Start()
               .AnchorNode()
               ->GetLayoutObject()
               ->Style()
               ->UserModify() == EUserModify::kReadWritePlaintextOnly) &&
      (selection.End().AnchorNode()->GetLayoutObject() &&
       selection.End().AnchorNode()->GetLayoutObject()->Style()->UserModify() ==
           EUserModify::kReadWritePlaintextOnly))
    match_style_ = false;

  if (match_style_) {
    insertion_style_ = EditingStyle::Create(selection.Start());
    insertion_style_->MergeTypingStyle(&GetDocument());
  }
}

void ReplaceSelectionCommand::InsertParagraphSeparatorIfNeeds(
    const VisibleSelection& selection,
    const ReplacementFragment& fragment,
    EditingState* editing_state) {
  const VisiblePosition visible_start = selection.VisibleStart();
  const VisiblePosition visible_end = selection.VisibleEnd();

  const bool selection_end_was_end_of_paragraph = IsEndOfParagraph(visible_end);
  const bool selection_start_was_start_of_paragraph =
      IsStartOfParagraph(visible_start);

  Element* const enclosing_block_of_visible_start =
      EnclosingBlock(visible_start.DeepEquivalent().AnchorNode());

  const bool start_is_inside_mail_blockquote = EnclosingNodeOfType(
      selection.Start(), IsMailHTMLBlockquoteElement, kCanCrossEditingBoundary);
  const bool selection_is_plain_text = !IsRichlyEditablePosition(selection.Base());
  Element* const current_root = selection.RootEditableElement();

  if ((selection_start_was_start_of_paragraph &&
       selection_end_was_end_of_paragraph &&
       !start_is_inside_mail_blockquote) ||
      enclosing_block_of_visible_start == current_root ||
      IsListItem(enclosing_block_of_visible_start) || selection_is_plain_text) {
    prevent_nesting_ = false;
  }

  if (selection.IsRange()) {
    // When the end of the selection being pasted into is at the end of a
    // paragraph, and that selection spans multiple blocks, not merging may
    // leave an empty line.
    // When the start of the selection being pasted into is at the start of a
    // block, not merging will leave hanging block(s).
    // Merge blocks if the start of the selection was in a Mail blockquote,
    // since we handle that case specially to prevent nesting.
    bool merge_blocks_after_delete = start_is_inside_mail_blockquote ||
                                     IsEndOfParagraph(visible_end) ||
                                     IsStartOfBlock(visible_start);
    // FIXME: We should only expand to include fully selected special elements
    // if we are copying a selection and pasting it on top of itself.
    if (!DeleteSelection(editing_state, DeleteSelectionOptions::Builder()
                                            .SetMergeBlocksAfterDelete(
                                                merge_blocks_after_delete)
                                            .SetSanitizeMarkup(true)
                                            .Build()))
      return;
    if (fragment.HasInterchangeNewlineAtStart()) {
      GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
      VisiblePosition start_after_delete =
          EndingVisibleSelection().VisibleStart();
      if (IsEndOfParagraph(start_after_delete) &&
          !IsStartOfParagraph(start_after_delete) &&
          !IsEndOfEditableOrNonEditableContent(start_after_delete)) {
        SetEndingSelection(SelectionForUndoStep::From(
            SelectionInDOMTree::Builder()
                .Collapse(NextPositionOf(start_after_delete).DeepEquivalent())
                .Build()));
      } else {
        InsertParagraphSeparator(editing_state);
      }
      if (editing_state->IsAborted())
        return;
    }
  } else {
    DCHECK(selection.IsCaret());
    if (fragment.HasInterchangeNewlineAtStart()) {
      const VisiblePosition next =
          NextPositionOf(visible_start, kCannotCrossEditingBoundary);
      if (IsEndOfParagraph(visible_start) &&
          !IsStartOfParagraph(visible_start) && next.IsNotNull()) {
        SetEndingSelection(
            SelectionForUndoStep::From(SelectionInDOMTree::Builder()
                                           .Collapse(next.DeepEquivalent())
                                           .Build()));
      } else {
        InsertParagraphSeparator(editing_state);
        if (editing_state->IsAborted())
          return;
        GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
      }
    }
    // We split the current paragraph in two to avoid nesting the blocks from
    // the fragment inside the current block.
    //
    // For example, paste
    //   <div>foo</div><div>bar</div><div>baz</div>
    // into
    //   <div>x^x</div>
    // where ^ is the caret.
    //
    // As long as the div styles are the same, visually you'd expect:
    //   <div>xbar</div><div>bar</div><div>bazx</div>
    // not
    //   <div>xbar<div>bar</div><div>bazx</div></div>
    // Don't do this if the selection started in a Mail blockquote.
    const VisiblePosition visible_start_position =
        EndingVisibleSelection().VisibleStart();
    if (prevent_nesting_ && !start_is_inside_mail_blockquote &&
        !IsEndOfParagraph(visible_start_position) &&
        !IsStartOfParagraph(visible_start_position)) {
      InsertParagraphSeparator(editing_state);
      if (editing_state->IsAborted())
        return;
      GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
      SetEndingSelection(SelectionForUndoStep::From(
          SelectionInDOMTree::Builder()
              .Collapse(
                  PreviousPositionOf(EndingVisibleSelection().VisibleStart())
                      .DeepEquivalent())
              .Build()));
    }
  }
}

void ReplaceSelectionCommand::DoApply(EditingState* editing_state) {
  TRACE_EVENT0("blink", "ReplaceSelectionCommand::doApply");
  const VisibleSelection& selection = EndingVisibleSelection();

  // ReplaceSelectionCommandTest.CrashWithNoSelection hits below abort
  // condition.
  ABORT_EDITING_COMMAND_IF(selection.IsNone());
  ABORT_EDITING_COMMAND_IF(!selection.IsValidFor(GetDocument()));

  if (!selection.RootEditableElement())
    return;

  ReplacementFragment fragment(&GetDocument(), document_fragment_.Get(),
                               selection);
  bool trivial_replace_result = PerformTrivialReplace(fragment, editing_state);
  if (editing_state->IsAborted())
    return;
  if (trivial_replace_result)
    return;

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  SetUpStyle(selection);
  Element* const current_root = selection.RootEditableElement();
  const bool start_is_inside_mail_blockquote = EnclosingNodeOfType(
      selection.Start(), IsMailHTMLBlockquoteElement, kCanCrossEditingBoundary);
  const bool selection_is_plain_text =
      !IsRichlyEditablePosition(selection.Base());
  const bool selection_end_was_end_of_paragraph =
      IsEndOfParagraph(selection.VisibleEnd());
  const bool selection_start_was_start_of_paragraph =
      IsStartOfParagraph(selection.VisibleStart());
  InsertParagraphSeparatorIfNeeds(selection, fragment, editing_state);
  if (editing_state->IsAborted())
    return;

  Position insertion_pos = EndingVisibleSelection().Start();
  // We don't want any of the pasted content to end up nested in a Mail
  // blockquote, so first break out of any surrounding Mail blockquotes. Unless
  // we're inserting in a table, in which case breaking the blockquote will
  // prevent the content from actually being inserted in the table.
  if (EnclosingNodeOfType(insertion_pos, IsMailHTMLBlockquoteElement,
                          kCanCrossEditingBoundary) &&
      prevent_nesting_ &&
      !(EnclosingNodeOfType(insertion_pos, &IsTableStructureNode))) {
    ApplyCommandToComposite(BreakBlockquoteCommand::Create(GetDocument()),
                            editing_state);
    if (editing_state->IsAborted())
      return;
    // This will leave a br between the split.
    Node* br = EndingVisibleSelection().Start().AnchorNode();
    DCHECK(IsHTMLBRElement(br)) << br;
    // Insert content between the two blockquotes, but remove the br (since it
    // was just a placeholder).
    insertion_pos = Position::InParentBeforeNode(*br);
    RemoveNode(br, editing_state);
    if (editing_state->IsAborted())
      return;
  }

  // Inserting content could cause whitespace to collapse, e.g. inserting
  // <div>foo</div> into hello^ world.
  PrepareWhitespaceAtPositionForSplit(insertion_pos);

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // If the downstream node has been removed there's no point in continuing.
  if (!MostForwardCaretPosition(insertion_pos).AnchorNode())
    return;

  // NOTE: This would be an incorrect usage of downstream() if downstream() were
  // changed to mean the last position after p that maps to the same visible
  // position as p (since in the case where a br is at the end of a block and
  // collapsed away, there are positions after the br which map to the same
  // visible position as [br, 0]).
  HTMLBRElement* end_br = ToHTMLBRElementOrNull(
      *MostForwardCaretPosition(insertion_pos).AnchorNode());
  VisiblePosition original_vis_pos_before_end_br;
  if (end_br) {
    original_vis_pos_before_end_br =
        PreviousPositionOf(VisiblePosition::BeforeNode(*end_br));
  }

  Element* enclosing_block_of_insertion_pos =
      EnclosingBlock(insertion_pos.AnchorNode());

  // Adjust |enclosingBlockOfInsertionPos| to prevent nesting.
  // If the start was in a Mail blockquote, we will have already handled
  // adjusting |enclosingBlockOfInsertionPos| above.
  if (prevent_nesting_ && enclosing_block_of_insertion_pos &&
      enclosing_block_of_insertion_pos != current_root &&
      !IsTableCell(enclosing_block_of_insertion_pos) &&
      !start_is_inside_mail_blockquote) {
    VisiblePosition visible_insertion_pos =
        CreateVisiblePosition(insertion_pos);
    if (IsEndOfBlock(visible_insertion_pos) &&
        !(IsStartOfBlock(visible_insertion_pos) &&
          fragment.HasInterchangeNewlineAtEnd()))
      insertion_pos =
          Position::InParentAfterNode(*enclosing_block_of_insertion_pos);
    else if (IsStartOfBlock(visible_insertion_pos))
      insertion_pos =
          Position::InParentBeforeNode(*enclosing_block_of_insertion_pos);
  }

  // Paste at start or end of link goes outside of link.
  insertion_pos =
      PositionAvoidingSpecialElementBoundary(insertion_pos, editing_state);
  if (editing_state->IsAborted())
    return;

  // FIXME: Can this wait until after the operation has been performed?  There
  // doesn't seem to be any work performed after this that queries or uses the
  // typing style.
  if (LocalFrame* frame = GetDocument().GetFrame())
    frame->GetEditor().ClearTypingStyle();

  RemoveHeadContents(fragment);

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // We don't want the destination to end up inside nodes that weren't selected.
  // To avoid that, we move the position forward without changing the visible
  // position so we're still at the same visible location, but outside of
  // preceding tags.
  insertion_pos = PositionAvoidingPrecedingNodes(insertion_pos);

  // Paste into run of tabs splits the tab span.
  insertion_pos = PositionOutsideTabSpan(insertion_pos);

  HandleStyleSpansBeforeInsertion(fragment, insertion_pos);

  // We're finished if there is nothing to add.
  if (fragment.IsEmpty() || !fragment.FirstChild())
    return;

  // If we are not trying to match the destination style we prefer a position
  // that is outside inline elements that provide style.
  // This way we can produce a less verbose markup.
  // We can skip this optimization for fragments not wrapped in one of
  // our style spans and for positions inside list items
  // since insertAsListItems already does the right thing.
  if (!match_style_ && !EnclosingList(insertion_pos.ComputeContainerNode())) {
    if (insertion_pos.ComputeContainerNode()->IsTextNode() &&
        insertion_pos.OffsetInContainerNode() &&
        !insertion_pos.AtLastEditingPositionForNode()) {
      SplitTextNode(ToText(insertion_pos.ComputeContainerNode()),
                    insertion_pos.OffsetInContainerNode());
      insertion_pos =
          Position::FirstPositionInNode(*insertion_pos.ComputeContainerNode());
    }

    if (HTMLElement* element_to_split_to =
            ElementToSplitToAvoidPastingIntoInlineElementsWithStyle(
                insertion_pos)) {
      if (insertion_pos.ComputeContainerNode() !=
          element_to_split_to->parentNode()) {
        Node* split_start = insertion_pos.ComputeNodeAfterPosition();
        if (!split_start)
          split_start = insertion_pos.ComputeContainerNode();
        Node* node_to_split_to =
            SplitTreeToNode(split_start, element_to_split_to->parentNode());
        insertion_pos = Position::InParentBeforeNode(*node_to_split_to);
      }
    }
  }

  // FIXME: When pasting rich content we're often prevented from heading down
  // the fast path by style spans.  Try again here if they've been removed.

  // 1) Insert the content.
  // 2) Remove redundant styles and style tags, this inner <b> for example:
  // <b>foo <b>bar</b> baz</b>.
  // 3) Merge the start of the added content with the content before the
  //    position being pasted into.
  // 4) Do one of the following:
  //    a) expand the last br if the fragment ends with one and it collapsed,
  //    b) merge the last paragraph of the incoming fragment with the paragraph
  //       that contained the end of the selection that was pasted into, or
  //    c) handle an interchange newline at the end of the incoming fragment.
  // 5) Add spaces for smart replace.
  // 6) Select the replacement if requested, and match style if requested.

  InsertedNodes inserted_nodes;
  inserted_nodes.SetRefNode(fragment.FirstChild());
  DCHECK(inserted_nodes.RefNode());
  Node* node = inserted_nodes.RefNode()->nextSibling();

  fragment.RemoveNode(inserted_nodes.RefNode());

  Element* block_start = EnclosingBlock(insertion_pos.AnchorNode());
  if ((IsHTMLListElement(inserted_nodes.RefNode()) ||
       (IsHTMLListElement(inserted_nodes.RefNode()->firstChild()))) &&
      block_start && block_start->GetLayoutObject()->IsListItem() &&
      HasEditableStyle(*block_start->parentNode())) {
    inserted_nodes.SetRefNode(
        InsertAsListItems(ToHTMLElement(inserted_nodes.RefNode()), block_start,
                          insertion_pos, inserted_nodes, editing_state));
    if (editing_state->IsAborted())
      return;
  } else {
    InsertNodeAt(inserted_nodes.RefNode(), insertion_pos, editing_state);
    if (editing_state->IsAborted())
      return;
    inserted_nodes.RespondToNodeInsertion(*inserted_nodes.RefNode());
  }

  // Mutation events (bug 22634) may have already removed the inserted content
  if (!inserted_nodes.RefNode()->isConnected())
    return;

  bool plain_text_fragment = IsPlainTextMarkup(inserted_nodes.RefNode());

  while (node) {
    Node* next = node->nextSibling();
    fragment.RemoveNode(node);
    InsertNodeAfter(node, inserted_nodes.RefNode(), editing_state);
    if (editing_state->IsAborted())
      return;
    inserted_nodes.RespondToNodeInsertion(*node);

    // Mutation events (bug 22634) may have already removed the inserted content
    if (!node->isConnected())
      return;

    inserted_nodes.SetRefNode(node);
    if (node && plain_text_fragment)
      plain_text_fragment = IsPlainTextMarkup(node);
    node = next;
  }

  if (IsRichlyEditablePosition(insertion_pos)) {
    RemoveUnrenderedTextNodesAtEnds(inserted_nodes);
    ABORT_EDITING_COMMAND_IF(!inserted_nodes.RefNode());
  }

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Mutation events (bug 20161) may have already removed the inserted content
  if (!inserted_nodes.FirstNodeInserted() ||
      !inserted_nodes.FirstNodeInserted()->isConnected())
    return;

  // Scripts specified in javascript protocol may remove
  // |enclosingBlockOfInsertionPos| during insertion, e.g. <iframe
  // src="javascript:...">
  if (enclosing_block_of_insertion_pos &&
      !enclosing_block_of_insertion_pos->isConnected())
    enclosing_block_of_insertion_pos = nullptr;

  VisiblePosition start_of_inserted_content = CreateVisiblePosition(
      FirstPositionInOrBeforeNode(*inserted_nodes.FirstNodeInserted()));

  // We inserted before the enclosingBlockOfInsertionPos to prevent nesting, and
  // the content before the enclosingBlockOfInsertionPos wasn't in its own block
  // and didn't have a br after it, so the inserted content ended up in the same
  // paragraph.
  if (!start_of_inserted_content.IsNull() && enclosing_block_of_insertion_pos &&
      insertion_pos.AnchorNode() ==
          enclosing_block_of_insertion_pos->parentNode() &&
      (unsigned)insertion_pos.ComputeEditingOffset() <
          enclosing_block_of_insertion_pos->NodeIndex() &&
      !IsStartOfParagraph(start_of_inserted_content)) {
    InsertNodeAt(HTMLBRElement::Create(GetDocument()),
                 start_of_inserted_content.DeepEquivalent(), editing_state);
    if (editing_state->IsAborted())
      return;
  }

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (end_br &&
      (plain_text_fragment ||
       (ShouldRemoveEndBR(end_br, original_vis_pos_before_end_br) &&
        !(fragment.HasInterchangeNewlineAtEnd() && selection_is_plain_text)))) {
    ContainerNode* parent = end_br->parentNode();
    inserted_nodes.WillRemoveNode(*end_br);
    ABORT_EDITING_COMMAND_IF(!inserted_nodes.RefNode());
    RemoveNode(end_br, editing_state);
    if (editing_state->IsAborted())
      return;
    if (Node* node_to_remove = HighestNodeToRemoveInPruning(parent)) {
      inserted_nodes.WillRemoveNode(*node_to_remove);
      ABORT_EDITING_COMMAND_IF(!inserted_nodes.RefNode());
      RemoveNode(node_to_remove, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }

  MakeInsertedContentRoundTrippableWithHTMLTreeBuilder(inserted_nodes,
                                                       editing_state);
  if (editing_state->IsAborted())
    return;

  {
    // TODO(dominicc): refNode may not be connected, for example in
    // LayoutTests/editing/inserting/insert-table-in-paragraph-crash.html .
    // Refactor this so there's a relationship between the conditions
    // where refNode is dereferenced and refNode is connected.
    bool ref_node_was_connected = inserted_nodes.RefNode()->isConnected();
    RemoveRedundantStylesAndKeepStyleSpanInline(inserted_nodes, editing_state);
    if (editing_state->IsAborted())
      return;
    DCHECK_EQ(inserted_nodes.RefNode()->isConnected(), ref_node_was_connected)
        << inserted_nodes.RefNode();
  }

  if (sanitize_fragment_ && inserted_nodes.FirstNodeInserted()) {
    ApplyCommandToComposite(
        SimplifyMarkupCommand::Create(GetDocument(),
                                      inserted_nodes.FirstNodeInserted(),
                                      inserted_nodes.PastLastLeaf()),
        editing_state);
    if (editing_state->IsAborted())
      return;
  }

  // Setup |start_of_inserted_content_| and |end_of_inserted_content_|.
  // This should be the last two lines of code that access insertedNodes.
  // TODO(editing-dev): The {First,Last}NodeInserted() nullptr checks may be
  // unnecessary. Investigate.
  start_of_inserted_content_ =
      inserted_nodes.FirstNodeInserted()
          ? FirstPositionInOrBeforeNode(*inserted_nodes.FirstNodeInserted())
          : Position();
  end_of_inserted_content_ =
      inserted_nodes.LastLeafInserted()
          ? LastPositionInOrAfterNode(*inserted_nodes.LastLeafInserted())
          : Position();

  // Determine whether or not we should merge the end of inserted content with
  // what's after it before we do the start merge so that the start merge
  // doesn't effect our decision.
  should_merge_end_ = ShouldMergeEnd(selection_end_was_end_of_paragraph);

  if (ShouldMergeStart(selection_start_was_start_of_paragraph,
                       fragment.HasInterchangeNewlineAtStart(),
                       start_is_inside_mail_blockquote)) {
    VisiblePosition start_of_paragraph_to_move =
        PositionAtStartOfInsertedContent();
    VisiblePosition destination =
        PreviousPositionOf(start_of_paragraph_to_move);

    // Helpers for making the VisiblePositions valid again after DOM changes.
    PositionWithAffinity start_of_paragraph_to_move_position =
        start_of_paragraph_to_move.ToPositionWithAffinity();
    PositionWithAffinity destination_position =
        destination.ToPositionWithAffinity();

    // We need to handle the case where we need to merge the end
    // but our destination node is inside an inline that is the last in the
    // block.
    // We insert a placeholder before the newly inserted content to avoid being
    // merged into the inline.
    Node* destination_node = destination.DeepEquivalent().AnchorNode();
    if (should_merge_end_ &&
        destination_node != EnclosingInline(destination_node) &&
        EnclosingInline(destination_node)->nextSibling()) {
      InsertNodeBefore(HTMLBRElement::Create(GetDocument()),
                       inserted_nodes.RefNode(), editing_state);
      if (editing_state->IsAborted())
        return;
    }

    // Merging the the first paragraph of inserted content with the content that
    // came before the selection that was pasted into would also move content
    // after the selection that was pasted into if: only one paragraph was being
    // pasted, and it was not wrapped in a block, the selection that was pasted
    // into ended at the end of a block and the next paragraph didn't start at
    // the start of a block.
    // Insert a line break just after the inserted content to separate it from
    // what comes after and prevent that from happening.
    VisiblePosition end_of_inserted_content = PositionAtEndOfInsertedContent();
    if (StartOfParagraph(end_of_inserted_content).DeepEquivalent() ==
        start_of_paragraph_to_move_position.GetPosition()) {
      InsertNodeAt(HTMLBRElement::Create(GetDocument()),
                   end_of_inserted_content.DeepEquivalent(), editing_state);
      if (editing_state->IsAborted())
        return;
      // Mutation events (bug 22634) triggered by inserting the <br> might have
      // removed the content we're about to move
      if (!start_of_paragraph_to_move_position.IsConnected())
        return;
    }

    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    // Making the two VisiblePositions valid again.
    start_of_paragraph_to_move =
        CreateVisiblePosition(start_of_paragraph_to_move_position);
    destination = CreateVisiblePosition(destination_position);

    // FIXME: Maintain positions for the start and end of inserted content
    // instead of keeping nodes.  The nodes are only ever used to create
    // positions where inserted content starts/ends.
    MoveParagraph(start_of_paragraph_to_move,
                  EndOfParagraph(start_of_paragraph_to_move), destination,
                  editing_state);
    if (editing_state->IsAborted())
      return;

    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
    const VisibleSelection& visible_selection_of_insterted_content =
        EndingVisibleSelection();
    start_of_inserted_content_ = MostForwardCaretPosition(
        visible_selection_of_insterted_content.VisibleStart().DeepEquivalent());
    if (end_of_inserted_content_.IsOrphan()) {
      end_of_inserted_content_ = MostBackwardCaretPosition(
          visible_selection_of_insterted_content.VisibleEnd().DeepEquivalent());
    }
  }

  Position last_position_to_select;
  if (fragment.HasInterchangeNewlineAtEnd()) {
    VisiblePosition end_of_inserted_content = PositionAtEndOfInsertedContent();
    VisiblePosition next =
        NextPositionOf(end_of_inserted_content, kCannotCrossEditingBoundary);

    if (selection_end_was_end_of_paragraph ||
        !IsEndOfParagraph(end_of_inserted_content) || next.IsNull()) {
      if (TextControlElement* text_control =
              EnclosingTextControl(current_root)) {
        if (!inserted_nodes.LastLeafInserted()->nextSibling()) {
          InsertNodeAfter(text_control->CreatePlaceholderBreakElement(),
                          inserted_nodes.LastLeafInserted(), editing_state);
          if (editing_state->IsAborted())
            return;
        }
        SetEndingSelection(SelectionForUndoStep::From(
            SelectionInDOMTree::Builder()
                .Collapse(
                    Position::AfterNode(*inserted_nodes.LastLeafInserted()))
                .Build()));
        // Select up to the paragraph separator that was added.
        last_position_to_select =
            EndingVisibleSelection().VisibleStart().DeepEquivalent();
      } else if (!IsStartOfParagraph(end_of_inserted_content)) {
        SetEndingSelection(SelectionForUndoStep::From(
            SelectionInDOMTree::Builder()
                .Collapse(end_of_inserted_content.DeepEquivalent())
                .Build()));
        Element* enclosing_block_element = EnclosingBlock(
            end_of_inserted_content.DeepEquivalent().AnchorNode());
        if (IsListItem(enclosing_block_element)) {
          HTMLLIElement* new_list_item = HTMLLIElement::Create(GetDocument());
          InsertNodeAfter(new_list_item, enclosing_block_element,
                          editing_state);
          if (editing_state->IsAborted())
            return;
          SetEndingSelection(SelectionForUndoStep::From(
              SelectionInDOMTree::Builder()
                  .Collapse(Position::FirstPositionInNode(*new_list_item))
                  .Build()));
        } else {
          // Use a default paragraph element (a plain div) for the empty
          // paragraph, using the last paragraph block's style seems to annoy
          // users.
          InsertParagraphSeparator(
              editing_state, true,
              !start_is_inside_mail_blockquote &&
                  HighestEnclosingNodeOfType(
                      end_of_inserted_content.DeepEquivalent(),
                      IsMailHTMLBlockquoteElement, kCannotCrossEditingBoundary,
                      inserted_nodes.FirstNodeInserted()->parentNode()));
          if (editing_state->IsAborted())
            return;
        }

        GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

        // Select up to the paragraph separator that was added.
        last_position_to_select =
            EndingVisibleSelection().VisibleStart().DeepEquivalent();
        UpdateNodesInserted(last_position_to_select.AnchorNode());
      }
    } else {
      // Select up to the beginning of the next paragraph.
      last_position_to_select = MostForwardCaretPosition(next.DeepEquivalent());
    }
  } else {
    MergeEndIfNeeded(editing_state);
    if (editing_state->IsAborted())
      return;
  }

  if (ShouldPerformSmartReplace()) {
    AddSpacesForSmartReplace(editing_state);
    if (editing_state->IsAborted())
      return;
  }

  // If we are dealing with a fragment created from plain text
  // no style matching is necessary.
  if (plain_text_fragment)
    match_style_ = false;

  CompleteHTMLReplacement(last_position_to_select, editing_state);
}

bool ReplaceSelectionCommand::ShouldRemoveEndBR(
    HTMLBRElement* end_br,
    const VisiblePosition& original_vis_pos_before_end_br) {
  if (!end_br || !end_br->isConnected())
    return false;

  VisiblePosition visible_pos = VisiblePosition::BeforeNode(*end_br);

  // Don't remove the br if nothing was inserted.
  if (PreviousPositionOf(visible_pos).DeepEquivalent() ==
      original_vis_pos_before_end_br.DeepEquivalent())
    return false;

  // Remove the br if it is collapsed away and so is unnecessary.
  if (!GetDocument().InNoQuirksMode() && IsEndOfBlock(visible_pos) &&
      !IsStartOfParagraph(visible_pos))
    return true;

  // A br that was originally holding a line open should be displaced by
  // inserted content or turned into a line break.
  // A br that was originally acting as a line break should still be acting as a
  // line break, not as a placeholder.
  return IsStartOfParagraph(visible_pos) && IsEndOfParagraph(visible_pos);
}

bool ReplaceSelectionCommand::ShouldPerformSmartReplace() const {
  if (!smart_replace_)
    return false;

  TextControlElement* text_control =
      EnclosingTextControl(PositionAtStartOfInsertedContent().DeepEquivalent());
  if (IsHTMLInputElement(text_control) &&
      ToHTMLInputElement(text_control)->type() == InputTypeNames::password)
    return false;  // Disable smart replace for password fields.

  return true;
}

static bool IsCharacterSmartReplaceExemptConsideringNonBreakingSpace(
    UChar32 character,
    bool previous_character) {
  return IsCharacterSmartReplaceExempt(
      character == kNoBreakSpaceCharacter ? ' ' : character,
      previous_character);
}

void ReplaceSelectionCommand::AddSpacesForSmartReplace(
    EditingState* editing_state) {
  VisiblePosition end_of_inserted_content = PositionAtEndOfInsertedContent();
  Position end_upstream =
      MostBackwardCaretPosition(end_of_inserted_content.DeepEquivalent());
  Node* end_node = end_upstream.ComputeNodeBeforePosition();
  int end_offset =
      end_node && end_node->IsTextNode() ? ToText(end_node)->length() : 0;
  if (end_upstream.IsOffsetInAnchor()) {
    end_node = end_upstream.ComputeContainerNode();
    end_offset = end_upstream.OffsetInContainerNode();
  }

  bool needs_trailing_space =
      !IsEndOfParagraph(end_of_inserted_content) &&
      !IsCharacterSmartReplaceExemptConsideringNonBreakingSpace(
          CharacterAfter(end_of_inserted_content), false);
  if (needs_trailing_space && end_node) {
    bool collapse_white_space =
        !end_node->GetLayoutObject() ||
        end_node->GetLayoutObject()->Style()->CollapseWhiteSpace();
    if (end_node->IsTextNode()) {
      InsertTextIntoNode(ToText(end_node), end_offset,
                         collapse_white_space ? NonBreakingSpaceString() : " ");
      if (end_of_inserted_content_.ComputeContainerNode() == end_node)
        end_of_inserted_content_ = Position(
            end_node, end_of_inserted_content_.OffsetInContainerNode() + 1);
    } else {
      Text* node = GetDocument().CreateEditingTextNode(
          collapse_white_space ? NonBreakingSpaceString() : " ");
      InsertNodeAfter(node, end_node, editing_state);
      if (editing_state->IsAborted())
        return;
      // Make sure that |UpdateNodesInserted| does not change
      // |start_of_inserted_content|.
      DCHECK(start_of_inserted_content_.IsNotNull());
      UpdateNodesInserted(node);
    }
  }

  GetDocument().UpdateStyleAndLayout();

  VisiblePosition start_of_inserted_content =
      PositionAtStartOfInsertedContent();
  Position start_downstream =
      MostForwardCaretPosition(start_of_inserted_content.DeepEquivalent());
  Node* start_node = start_downstream.ComputeNodeAfterPosition();
  unsigned start_offset = 0;
  if (start_downstream.IsOffsetInAnchor()) {
    start_node = start_downstream.ComputeContainerNode();
    start_offset = start_downstream.OffsetInContainerNode();
  }

  bool needs_leading_space =
      !IsStartOfParagraph(start_of_inserted_content) &&
      !IsCharacterSmartReplaceExemptConsideringNonBreakingSpace(
          CharacterBefore(start_of_inserted_content), true);
  if (needs_leading_space && start_node) {
    bool collapse_white_space =
        !start_node->GetLayoutObject() ||
        start_node->GetLayoutObject()->Style()->CollapseWhiteSpace();
    if (start_node->IsTextNode()) {
      InsertTextIntoNode(ToText(start_node), start_offset,
                         collapse_white_space ? NonBreakingSpaceString() : " ");
      if (end_of_inserted_content_.ComputeContainerNode() == start_node &&
          end_of_inserted_content_.OffsetInContainerNode())
        end_of_inserted_content_ = Position(
            start_node, end_of_inserted_content_.OffsetInContainerNode() + 1);
    } else {
      Text* node = GetDocument().CreateEditingTextNode(
          collapse_white_space ? NonBreakingSpaceString() : " ");
      // Don't UpdateNodesInserted. Doing so would set end_of_inserted_content_
      // to be the node containing the leading space, but
      // end_of_inserted_content_ issupposed to mark the end of pasted content.
      InsertNodeBefore(node, start_node, editing_state);
      if (editing_state->IsAborted())
        return;
      start_of_inserted_content_ = Position::FirstPositionInNode(*node);
    }
  }
}

void ReplaceSelectionCommand::CompleteHTMLReplacement(
    const Position& last_position_to_select,
    EditingState* editing_state) {
  Position start = PositionAtStartOfInsertedContent().DeepEquivalent();
  Position end = PositionAtEndOfInsertedContent().DeepEquivalent();

  // Mutation events may have deleted start or end
  if (start.IsNotNull() && !start.IsOrphan() && end.IsNotNull() &&
      !end.IsOrphan()) {
    // FIXME (11475): Remove this and require that the creator of the fragment
    // to use nbsps.
    RebalanceWhitespaceAt(start);
    RebalanceWhitespaceAt(end);

    if (match_style_) {
      DCHECK(insertion_style_);
      // Since |ApplyStyle()| changes contents of anchor node of |start| and
      // |end|, we should relocate them.
      Range* const range = Range::Create(GetDocument(), start, end);
      ApplyStyle(insertion_style_.Get(), start, end, editing_state);
      start = range->StartPosition();
      end = range->EndPosition();
      range->Dispose();
      if (editing_state->IsAborted())
        return;
    }

    if (last_position_to_select.IsNotNull())
      end = last_position_to_select;

    MergeTextNodesAroundPosition(start, end, editing_state);
    if (editing_state->IsAborted())
      return;
  } else if (last_position_to_select.IsNotNull()) {
    start = end = last_position_to_select;
  } else {
    return;
  }

  start_of_inserted_range_ = start;
  end_of_inserted_range_ = end;

  if (select_replacement_) {
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .SetBaseAndExtentDeprecated(start, end)
            .Build()));
    return;
  }

  if (end.IsNotNull()) {
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(end)
            .Build()));
    return;
  }
  SetEndingSelection(SelectionForUndoStep());
}

void ReplaceSelectionCommand::MergeTextNodesAroundPosition(
    Position& position,
    Position& position_only_to_be_updated,
    EditingState* editing_state) {
  bool position_is_offset_in_anchor = position.IsOffsetInAnchor();
  bool position_only_to_be_updated_is_offset_in_anchor =
      position_only_to_be_updated.IsOffsetInAnchor();
  Text* text = nullptr;
  if (position_is_offset_in_anchor && position.ComputeContainerNode() &&
      position.ComputeContainerNode()->IsTextNode()) {
    text = ToText(position.ComputeContainerNode());
  } else {
    Node* before = position.ComputeNodeBeforePosition();
    if (before && before->IsTextNode()) {
      text = ToText(before);
    } else {
      Node* after = position.ComputeNodeAfterPosition();
      if (after && after->IsTextNode())
        text = ToText(after);
    }
  }
  if (!text)
    return;

  // Merging Text nodes causes an additional layout. We'd like to skip it if the
  // editable text is huge.
  // TODO(tkent): 1024 was chosen by my intuition.  We need data.
  const unsigned kMergeSizeLimit = 1024;
  bool has_incomplete_surrogate =
      text->data().length() >= 1 &&
      (U16_IS_TRAIL(text->data()[0]) ||
       U16_IS_LEAD(text->data()[text->data().length() - 1]));
  if (!has_incomplete_surrogate && text->data().length() > kMergeSizeLimit)
    return;
  if (text->previousSibling() && text->previousSibling()->IsTextNode()) {
    Text* previous = ToText(text->previousSibling());
    if (has_incomplete_surrogate ||
        previous->data().length() <= kMergeSizeLimit) {
      InsertTextIntoNode(text, 0, previous->data());

      if (position_is_offset_in_anchor) {
        position =
            Position(position.ComputeContainerNode(),
                     previous->length() + position.OffsetInContainerNode());
      } else {
        position = ComputePositionForNodeRemoval(position, *previous);
      }

      if (position_only_to_be_updated_is_offset_in_anchor) {
        if (position_only_to_be_updated.ComputeContainerNode() == text)
          position_only_to_be_updated = Position(
              text, previous->length() +
                        position_only_to_be_updated.OffsetInContainerNode());
        else if (position_only_to_be_updated.ComputeContainerNode() == previous)
          position_only_to_be_updated = Position(
              text, position_only_to_be_updated.OffsetInContainerNode());
      } else {
        position_only_to_be_updated = ComputePositionForNodeRemoval(
            position_only_to_be_updated, *previous);
      }

      RemoveNode(previous, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }
  if (text->nextSibling() && text->nextSibling()->IsTextNode()) {
    Text* next = ToText(text->nextSibling());
    if (!has_incomplete_surrogate && next->data().length() > kMergeSizeLimit)
      return;
    unsigned original_length = text->length();
    InsertTextIntoNode(text, original_length, next->data());

    if (!position_is_offset_in_anchor)
      position = ComputePositionForNodeRemoval(position, *next);

    if (position_only_to_be_updated_is_offset_in_anchor &&
        position_only_to_be_updated.ComputeContainerNode() == next) {
      position_only_to_be_updated = Position(
          text, original_length +
                    position_only_to_be_updated.OffsetInContainerNode());
    } else {
      position_only_to_be_updated =
          ComputePositionForNodeRemoval(position_only_to_be_updated, *next);
    }

    RemoveNode(next, editing_state);
    if (editing_state->IsAborted())
      return;
  }
}

InputEvent::InputType ReplaceSelectionCommand::GetInputType() const {
  // |ReplaceSelectionCommand| could be used with Paste, Drag&Drop,
  // InsertFragment and |TypingCommand|.
  // 1. Paste, Drag&Drop, InsertFragment should rely on correct |input_type_|.
  // 2. |TypingCommand| will supply the |GetInputType()|, so |input_type_| could
  //    default to |InputType::kNone|.
  return input_type_;
}

// If the user is inserting a list into an existing list, instead of nesting the
// list, we put the list items into the existing list.
Node* ReplaceSelectionCommand::InsertAsListItems(HTMLElement* list_element,
                                                 Element* insertion_block,
                                                 const Position& insert_pos,
                                                 InsertedNodes& inserted_nodes,
                                                 EditingState* editing_state) {
  while (list_element->HasOneChild() &&
         IsHTMLListElement(list_element->firstChild()))
    list_element = ToHTMLElement(list_element->firstChild());

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  bool is_start = IsStartOfParagraph(CreateVisiblePosition(insert_pos));
  bool is_end = IsEndOfParagraph(CreateVisiblePosition(insert_pos));
  bool is_middle = !is_start && !is_end;
  Node* last_node = insertion_block;

  // If we're in the middle of a list item, we should split it into two separate
  // list items and insert these nodes between them.
  if (is_middle) {
    int text_node_offset = insert_pos.OffsetInContainerNode();
    if (insert_pos.AnchorNode()->IsTextNode() && text_node_offset > 0)
      SplitTextNode(ToText(insert_pos.AnchorNode()), text_node_offset);
    SplitTreeToNode(insert_pos.AnchorNode(), last_node, true);
  }

  while (Node* list_item = list_element->firstChild()) {
    list_element->RemoveChild(list_item, ASSERT_NO_EXCEPTION);
    if (is_start || is_middle) {
      InsertNodeBefore(list_item, last_node, editing_state);
      if (editing_state->IsAborted())
        return nullptr;
      inserted_nodes.RespondToNodeInsertion(*list_item);
    } else if (is_end) {
      InsertNodeAfter(list_item, last_node, editing_state);
      if (editing_state->IsAborted())
        return nullptr;
      inserted_nodes.RespondToNodeInsertion(*list_item);
      last_node = list_item;
    } else {
      NOTREACHED();
    }
  }
  if (is_start || is_middle) {
    if (Node* node = last_node->previousSibling())
      return node;
  }
  return last_node;
}

void ReplaceSelectionCommand::UpdateNodesInserted(Node* node) {
  if (!node)
    return;

  if (start_of_inserted_content_.IsNull())
    start_of_inserted_content_ = FirstPositionInOrBeforeNode(*node);

  end_of_inserted_content_ =
      LastPositionInOrAfterNode(NodeTraversal::LastWithinOrSelf(*node));
}

// During simple pastes, where we're just pasting a text node into a run of
// text, we insert the text node directly into the text node that holds the
// selection.  This is much faster than the generalized code in
// ReplaceSelectionCommand, and works around
// <https://bugs.webkit.org/show_bug.cgi?id=6148> since we don't split text
// nodes.
bool ReplaceSelectionCommand::PerformTrivialReplace(
    const ReplacementFragment& fragment,
    EditingState* editing_state) {
  if (!fragment.FirstChild() || fragment.FirstChild() != fragment.LastChild() ||
      !fragment.FirstChild()->IsTextNode())
    return false;

  // FIXME: Would be nice to handle smart replace in the fast path.
  if (smart_replace_ || fragment.HasInterchangeNewlineAtStart() ||
      fragment.HasInterchangeNewlineAtEnd())
    return false;

  // e.g. when "bar" is inserted after "foo" in <div><u>foo</u></div>, "bar"
  // should not be underlined.
  if (ElementToSplitToAvoidPastingIntoInlineElementsWithStyle(
          EndingVisibleSelection().Start()))
    return false;

  // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  Node* node_after_insertion_pos =
      MostForwardCaretPosition(EndingSelection().End()).AnchorNode();
  Text* text_node = ToText(fragment.FirstChild());
  // Our fragment creation code handles tabs, spaces, and newlines, so we don't
  // have to worry about those here.

  Position start = EndingVisibleSelection().Start();
  Position end = ReplaceSelectedTextInNode(text_node->data());
  if (end.IsNull())
    return false;

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (node_after_insertion_pos && node_after_insertion_pos->parentNode() &&
      IsHTMLBRElement(*node_after_insertion_pos) &&
      ShouldRemoveEndBR(
          ToHTMLBRElement(node_after_insertion_pos),
          VisiblePosition::BeforeNode(*node_after_insertion_pos))) {
    RemoveNodeAndPruneAncestors(node_after_insertion_pos, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  start_of_inserted_range_ = start;
  end_of_inserted_range_ = end;

  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtentDeprecated(select_replacement_ ? start : end, end)
          .Build()));

  return true;
}

bool ReplaceSelectionCommand::IsReplaceSelectionCommand() const {
  return true;
}

EphemeralRange ReplaceSelectionCommand::InsertedRange() const {
  return EphemeralRange(start_of_inserted_range_, end_of_inserted_range_);
}

void ReplaceSelectionCommand::Trace(blink::Visitor* visitor) {
  visitor->Trace(start_of_inserted_content_);
  visitor->Trace(end_of_inserted_content_);
  visitor->Trace(insertion_style_);
  visitor->Trace(document_fragment_);
  visitor->Trace(start_of_inserted_range_);
  visitor->Trace(end_of_inserted_range_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
