/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/serializers/styled_markup_serializer.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_style.h"
#include "third_party/blink/renderer/core/editing/editing_style_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

template <typename Strategy>
TextOffset ToTextOffset(const PositionTemplate<Strategy>& position) {
  if (position.IsNull())
    return TextOffset();

  auto* text_node = DynamicTo<Text>(position.ComputeContainerNode());
  if (!text_node)
    return TextOffset();

  return TextOffset(text_node, position.OffsetInContainerNode());
}

template <typename EditingStrategy>
static bool HandleSelectionBoundary(const Node&);

template <>
bool HandleSelectionBoundary<EditingStrategy>(const Node&) {
  return false;
}

template <>
bool HandleSelectionBoundary<EditingInFlatTreeStrategy>(const Node& node) {
  ShadowRoot* root = node.GetShadowRoot();
  return root && root->IsUserAgent();
}

}  // namespace

template <typename Strategy>
class StyledMarkupTraverser {
  STACK_ALLOCATED();

 public:
  StyledMarkupTraverser();
  StyledMarkupTraverser(StyledMarkupAccumulator*, Node*);
  StyledMarkupTraverser(const StyledMarkupTraverser&) = delete;
  StyledMarkupTraverser& operator=(const StyledMarkupTraverser&) = delete;

  Node* Traverse(Node*, Node*);
  void WrapWithNode(ContainerNode&, EditingStyle*);
  EditingStyle* CreateInlineStyleIfNeeded(Node&);

 private:
  bool ShouldAnnotate() const;
  bool ShouldConvertBlocksToInlines() const;
  bool IsForMarkupSanitization() const;
  void AppendStartMarkup(Node&);
  void AppendEndMarkup(Node&);
  EditingStyle* CreateInlineStyle(Element&);
  bool NeedsInlineStyle(const Element&);
  bool ShouldApplyWrappingStyle(const Node&) const;
  bool ContainsOnlyBRElement(const Element&) const;
  bool ShouldSerializeUnrenderedElement(const Node&) const;

  StyledMarkupAccumulator* accumulator_;
  Node* last_closed_;
  EditingStyle* wrapping_style_;
};

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::ShouldAnnotate() const {
  return accumulator_->ShouldAnnotate();
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::IsForMarkupSanitization() const {
  return accumulator_ && accumulator_->IsForMarkupSanitization();
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::ShouldConvertBlocksToInlines() const {
  return accumulator_->ShouldConvertBlocksToInlines();
}

template <typename Strategy>
StyledMarkupSerializer<Strategy>::StyledMarkupSerializer(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    Node* highest_node_to_be_serialized,
    const CreateMarkupOptions& options)
    : start_(start),
      end_(end),
      highest_node_to_be_serialized_(highest_node_to_be_serialized),
      options_(options),
      last_closed_(highest_node_to_be_serialized),
      wrapping_style_(nullptr) {}

template <typename Strategy>
static bool NeedInterchangeNewlineAfter(
    const VisiblePositionTemplate<Strategy>& v) {
  const VisiblePositionTemplate<Strategy> next = NextPositionOf(v);
  Node* upstream_node =
      MostBackwardCaretPosition(next.DeepEquivalent()).AnchorNode();
  Node* downstream_node =
      MostForwardCaretPosition(v.DeepEquivalent()).AnchorNode();
  // Add an interchange newline if a paragraph break is selected and a br won't
  // already be added to the markup to represent it.
  return IsEndOfParagraph(v) && IsStartOfParagraph(next) &&
         !(IsA<HTMLBRElement>(*upstream_node) &&
           upstream_node == downstream_node);
}

template <typename Strategy>
static bool NeedInterchangeNewlineAt(
    const VisiblePositionTemplate<Strategy>& v) {
  return NeedInterchangeNewlineAfter(PreviousPositionOf(v));
}

static EditingStyle* StyleFromMatchedRulesAndInlineDecl(
    const HTMLElement* element) {
  EditingStyle* style =
      MakeGarbageCollected<EditingStyle>(element->InlineStyle());
  // FIXME: Having to const_cast here is ugly, but it is quite a bit of work to
  // untangle the non-const-ness of styleFromMatchedRulesForElement.
  style->MergeStyleFromRules(const_cast<HTMLElement*>(element));
  return style;
}

template <typename Strategy>
String StyledMarkupSerializer<Strategy>::CreateMarkup() {
  StyledMarkupAccumulator markup_accumulator(
      ToTextOffset(start_.ParentAnchoredEquivalent()),
      ToTextOffset(end_.ParentAnchoredEquivalent()), start_.GetDocument(),
      options_);

  Node* past_end = end_.NodeAsRangePastLastNode();

  Node* first_node = start_.NodeAsRangeFirstNode();
  const VisiblePositionTemplate<Strategy> visible_start =
      CreateVisiblePosition(start_);
  const VisiblePositionTemplate<Strategy> visible_end =
      CreateVisiblePosition(end_);
  if (ShouldAnnotate() && NeedInterchangeNewlineAfter(visible_start)) {
    markup_accumulator.AppendInterchangeNewline();
    if (visible_start.DeepEquivalent() ==
        PreviousPositionOf(visible_end).DeepEquivalent())
      return markup_accumulator.TakeResults();

    first_node = NextPositionOf(visible_start).DeepEquivalent().AnchorNode();

    if (past_end && PositionTemplate<Strategy>::BeforeNode(*first_node)
                            .CompareTo(PositionTemplate<Strategy>::BeforeNode(
                                *past_end)) >= 0) {
      // This condition hits in editing/pasteboard/copy-display-none.html.
      return markup_accumulator.TakeResults();
    }
  }

  // If there is no the highest node in the selected nodes, |last_closed_| can
  // be #text when its parent is a formatting tag. In this case, #text is
  // wrapped by <span> tag, but this text should be wrapped by the formatting
  // tag. See http://crbug.com/634482
  bool should_append_parent_tag = false;
  if (!last_closed_) {
    last_closed_ =
        StyledMarkupTraverser<Strategy>().Traverse(first_node, past_end);
    if (last_closed_ && last_closed_->IsTextNode() &&
        IsPresentationalHTMLElement(last_closed_->parentNode())) {
      last_closed_ = last_closed_->parentElement();
      should_append_parent_tag = true;
    }
  }

  StyledMarkupTraverser<Strategy> traverser(&markup_accumulator, last_closed_);
  Node* last_closed = traverser.Traverse(first_node, past_end);

  if (highest_node_to_be_serialized_ && last_closed) {
    // TODO(hajimehoshi): This is calculated at createMarkupInternal too.
    Node* common_ancestor = Strategy::CommonAncestor(
        *start_.ComputeContainerNode(), *end_.ComputeContainerNode());
    DCHECK(common_ancestor);
    auto* body = To<HTMLBodyElement>(EnclosingElementWithTag(
        Position::FirstPositionInNode(*common_ancestor), html_names::kBodyTag));
    HTMLBodyElement* fully_selected_root = nullptr;
    // FIXME: Do this for all fully selected blocks, not just the body.
    if (body && AreSameRanges(body, start_, end_))
      fully_selected_root = body;

    // Also include all of the ancestors of lastClosed up to this special
    // ancestor.
    // FIXME: What is ancestor?
    for (ContainerNode* ancestor = Strategy::Parent(*last_closed); ancestor;
         ancestor = Strategy::Parent(*ancestor)) {
      if (ancestor == fully_selected_root &&
          !markup_accumulator.ShouldConvertBlocksToInlines()) {
        EditingStyle* fully_selected_root_style =
            StyleFromMatchedRulesAndInlineDecl(fully_selected_root);

        // Bring the background attribute over, but not as an attribute because
        // a background attribute on a div appears to have no effect.
        if ((!fully_selected_root_style ||
             !fully_selected_root_style->Style() ||
             !fully_selected_root_style->Style()->GetPropertyCSSValue(
                 CSSPropertyID::kBackgroundImage)) &&
            fully_selected_root->FastHasAttribute(
                html_names::kBackgroundAttr)) {
          fully_selected_root_style->Style()->ParseAndSetProperty(
              CSSPropertyID::kBackgroundImage,
              String("url('" +
                     fully_selected_root->getAttribute(
                         html_names::kBackgroundAttr) +
                     "')"),
              /* important */ false,
              fully_selected_root->GetExecutionContext()
                  ->GetSecureContextMode());
        }

        if (fully_selected_root_style->Style()) {
          // Reset the CSS properties to avoid an assertion error in
          // addStyleMarkup(). This assertion is caused at least when we select
          // all text of a <body> element whose 'text-decoration' property is
          // "inherit", and copy it.
          if (!PropertyMissingOrEqualToNone(fully_selected_root_style->Style(),
                                            CSSPropertyID::kTextDecoration)) {
            fully_selected_root_style->Style()->SetLonghandProperty(
                CSSPropertyID::kTextDecoration, CSSValueID::kNone);
          }
          if (!PropertyMissingOrEqualToNone(
                  fully_selected_root_style->Style(),
                  CSSPropertyID::kWebkitTextDecorationsInEffect)) {
            fully_selected_root_style->Style()->SetLonghandProperty(
                CSSPropertyID::kWebkitTextDecorationsInEffect,
                CSSValueID::kNone);
          }
          markup_accumulator.WrapWithStyleNode(
              fully_selected_root_style->Style());
        }
      } else {
        EditingStyle* style = traverser.CreateInlineStyleIfNeeded(*ancestor);
        // Since this node and all the other ancestors are not in the selection
        // we want styles that affect the exterior of the node not to be not
        // included.  If the node is not fully selected by the range, then we
        // don't want to keep styles that affect its relationship to the nodes
        // around it only the ones that affect it and the nodes within it.
        if (style && style->Style())
          style->Style()->RemoveProperty(CSSPropertyID::kFloat);
        traverser.WrapWithNode(*ancestor, style);
      }

      if (ancestor == highest_node_to_be_serialized_)
        break;
    }
  } else if (should_append_parent_tag) {
    EditingStyle* style = traverser.CreateInlineStyleIfNeeded(*last_closed_);
    traverser.WrapWithNode(To<ContainerNode>(*last_closed_), style);
  }

  // FIXME: The interchange newline should be placed in the block that it's in,
  // not after all of the content, unconditionally.
  if (!(last_closed && IsA<HTMLBRElement>(*last_closed)) && ShouldAnnotate() &&
      NeedInterchangeNewlineAt(visible_end))
    markup_accumulator.AppendInterchangeNewline();

  return markup_accumulator.TakeResults();
}

template <typename Strategy>
StyledMarkupTraverser<Strategy>::StyledMarkupTraverser()
    : StyledMarkupTraverser(nullptr, nullptr) {}

template <typename Strategy>
StyledMarkupTraverser<Strategy>::StyledMarkupTraverser(
    StyledMarkupAccumulator* accumulator,
    Node* last_closed)
    : accumulator_(accumulator),
      last_closed_(last_closed),
      wrapping_style_(nullptr) {
  if (!accumulator_) {
    DCHECK_EQ(last_closed_, static_cast<decltype(last_closed_)>(nullptr));
    return;
  }
  if (!last_closed_)
    return;
  Element* parent = DynamicTo<Element>(Strategy::Parent(*last_closed_));
  if (!parent)
    return;
  if (ShouldAnnotate()) {
    wrapping_style_ =
        EditingStyleUtilities::CreateWrappingStyleForAnnotatedSerialization(
            parent);
    return;
  }
  wrapping_style_ =
      EditingStyleUtilities::CreateWrappingStyleForSerialization(parent);
}

template <typename Strategy>
Node* StyledMarkupTraverser<Strategy>::Traverse(Node* start_node,
                                                Node* past_end) {
  HeapVector<Member<ContainerNode>> ancestors_to_close;
  Node* next;
  Node* last_closed = nullptr;
  for (Node* n = start_node; n && n != past_end; n = next) {
    // If |n| is a selection boundary such as <input>, traverse the child
    // nodes in the DOM tree instead of the flat tree.
    if (HandleSelectionBoundary<Strategy>(*n)) {
      last_closed =
          StyledMarkupTraverser<EditingStrategy>(accumulator_, last_closed_)
              .Traverse(n, EditingStrategy::NextSkippingChildren(*n));
      next = EditingInFlatTreeStrategy::NextSkippingChildren(*n);
    } else {
      next = Strategy::Next(*n);
      if (IsEnclosingBlock(n) && CanHaveChildrenForEditing(n) &&
          next == past_end && !ContainsOnlyBRElement(To<Element>(*n))) {
        // Don't write out empty block containers that aren't fully selected
        // unless the block container only contains br element.
        continue;
      }

      auto* element = DynamicTo<Element>(n);
      if (n->GetLayoutObject() || ShouldSerializeUnrenderedElement(*n)) {
        // Add the node to the markup if we're not skipping the descendants
        AppendStartMarkup(*n);

        // If node has no children, close the tag now.
        if (Strategy::HasChildren(*n)) {
          if (next == past_end && ContainsOnlyBRElement(*element)) {
            // node is not fully selected and node contains only one br element
            // as child. Close the br tag now.
            AppendStartMarkup(*next);
            AppendEndMarkup(*next);
            last_closed = next;
          } else {
            ancestors_to_close.push_back(To<ContainerNode>(n));
          }
          continue;
        }
        AppendEndMarkup(*n);
        last_closed = n;
      } else {
        next = Strategy::NextSkippingChildren(*n);
        // Don't skip over pastEnd.
        if (past_end && Strategy::IsDescendantOf(*past_end, *n))
          next = past_end;
      }
    }

    // If we didn't insert open tag and there's no more siblings or we're at the
    // end of the traversal, take care of ancestors.
    // FIXME: What happens if we just inserted open tag and reached the end?
    if (Strategy::NextSibling(*n) && next != past_end)
      continue;

    // Close up the ancestors.
    while (!ancestors_to_close.empty()) {
      ContainerNode* ancestor = ancestors_to_close.back();
      DCHECK(ancestor);
      if (next && next != past_end &&
          Strategy::IsDescendantOf(*next, *ancestor))
        break;
      // Not at the end of the range, close ancestors up to sibling of next
      // node.
      AppendEndMarkup(*ancestor);
      last_closed = ancestor;
      ancestors_to_close.pop_back();
    }

    // Surround the currently accumulated markup with markup for ancestors we
    // never opened as we leave the subtree(s) rooted at those ancestors.
    ContainerNode* next_parent = next ? Strategy::Parent(*next) : nullptr;
    if (next == past_end || n == next_parent)
      continue;

    DCHECK(n);
    Node* last_ancestor_closed_or_self =
        (last_closed && Strategy::IsDescendantOf(*n, *last_closed))
            ? last_closed
            : n;
    for (ContainerNode* parent =
             Strategy::Parent(*last_ancestor_closed_or_self);
         parent && parent != next_parent; parent = Strategy::Parent(*parent)) {
      // All ancestors that aren't in the ancestorsToClose list should either be
      // a) unrendered:
      if (!parent->GetLayoutObject())
        continue;
      // or b) ancestors that we never encountered during a pre-order traversal
      // starting at startNode:
      DCHECK(start_node);
      DCHECK(Strategy::IsDescendantOf(*start_node, *parent));
      EditingStyle* style = CreateInlineStyleIfNeeded(*parent);
      WrapWithNode(*parent, style);
      last_closed = parent;
    }
  }

  return last_closed;
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::NeedsInlineStyle(const Element& element) {
  if (!element.IsHTMLElement())
    return false;
  if (ShouldAnnotate())
    return true;
  return ShouldConvertBlocksToInlines() && IsEnclosingBlock(&element);
}

template <typename Strategy>
void StyledMarkupTraverser<Strategy>::WrapWithNode(ContainerNode& node,
                                                   EditingStyle* style) {
  if (!accumulator_)
    return;
  StringBuilder markup;
  if (auto* document = DynamicTo<Document>(node)) {
    MarkupFormatter::AppendXMLDeclaration(markup, *document);
    accumulator_->PushMarkup(markup.ToString());
    return;
  }
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return;
  if (ShouldApplyWrappingStyle(*element) || NeedsInlineStyle(*element))
    accumulator_->AppendElementWithInlineStyle(markup, *element, style);
  else
    accumulator_->AppendElement(markup, *element);
  accumulator_->PushMarkup(markup.ToString());
  accumulator_->AppendEndTag(*element);
}

template <typename Strategy>
EditingStyle* StyledMarkupTraverser<Strategy>::CreateInlineStyleIfNeeded(
    Node& node) {
  if (!accumulator_)
    return nullptr;

  auto* element = DynamicTo<Element>(node);
  if (!element)
    return nullptr;
  EditingStyle* inline_style = CreateInlineStyle(*element);
  if (ShouldConvertBlocksToInlines() && IsEnclosingBlock(&node))
    inline_style->ForceInline();
  return inline_style;
}

template <typename Strategy>
void StyledMarkupTraverser<Strategy>::AppendStartMarkup(Node& node) {
  if (!accumulator_)
    return;
  switch (node.getNodeType()) {
    case Node::kTextNode: {
      auto& text = To<Text>(node);
      if (IsA<HTMLTextAreaElement>(text.parentElement())) {
        accumulator_->AppendText(text);
        break;
      }
      EditingStyle* inline_style = nullptr;
      if (ShouldApplyWrappingStyle(text)) {
        inline_style = wrapping_style_->Copy();
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content
        // can change its appearance.
        // Make sure spans are inline style in paste side e.g. span { display:
        // block }.
        inline_style->ForceInline();
        // FIXME: Should this be included in forceInline?
        inline_style->Style()->SetLonghandProperty(CSSPropertyID::kFloat,
                                                   CSSValueID::kNone);

        if (IsForMarkupSanitization()) {
          EditingStyleUtilities::StripUAStyleRulesForMarkupSanitization(
              inline_style);
        }
      }
      accumulator_->AppendTextWithInlineStyle(text, inline_style);
      break;
    }
    case Node::kElementNode: {
      auto& element = To<Element>(node);
      if ((element.IsHTMLElement() && ShouldAnnotate()) ||
          ShouldApplyWrappingStyle(element)) {
        EditingStyle* inline_style = CreateInlineStyle(element);
        accumulator_->AppendElementWithInlineStyle(element, inline_style);
        break;
      }
      accumulator_->AppendElement(element);
      break;
    }
    default:
      accumulator_->AppendStartMarkup(node);
      break;
  }
}

template <typename Strategy>
void StyledMarkupTraverser<Strategy>::AppendEndMarkup(Node& node) {
  auto* element = DynamicTo<Element>(node);
  if (!accumulator_ || !element)
    return;
  accumulator_->AppendEndTag(*element);
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::ShouldApplyWrappingStyle(
    const Node& node) const {
  return last_closed_ &&
         Strategy::Parent(*last_closed_) == Strategy::Parent(node) &&
         wrapping_style_ && wrapping_style_->Style();
}

template <typename Strategy>
EditingStyle* StyledMarkupTraverser<Strategy>::CreateInlineStyle(
    Element& element) {
  EditingStyle* inline_style = nullptr;

  if (ShouldApplyWrappingStyle(element)) {
    inline_style = wrapping_style_->Copy();
    inline_style->RemovePropertiesInElementDefaultStyle(&element);
    inline_style->RemoveStyleConflictingWithStyleOfElement(&element);
  } else {
    inline_style = MakeGarbageCollected<EditingStyle>();
  }

  if (element.IsStyledElement() && element.InlineStyle())
    inline_style->OverrideWithStyle(element.InlineStyle());

  auto* html_element = DynamicTo<HTMLElement>(element);
  if (html_element && ShouldAnnotate()) {
    inline_style->MergeStyleFromRulesForSerialization(html_element);
  }

  if (IsForMarkupSanitization())
    EditingStyleUtilities::StripUAStyleRulesForMarkupSanitization(inline_style);

  return inline_style;
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::ContainsOnlyBRElement(
    const Element& element) const {
  auto* const first_child = element.firstChild();
  if (!first_child)
    return false;
  return IsA<HTMLBRElement>(first_child) && first_child == element.lastChild();
}

template <typename Strategy>
bool StyledMarkupTraverser<Strategy>::ShouldSerializeUnrenderedElement(
    const Node& node) const {
  DCHECK(!node.GetLayoutObject());
  if (node.IsElementNode() && To<Element>(node).HasDisplayContentsStyle())
    return true;
  if (EnclosingElementWithTag(FirstPositionInOrBeforeNode(node),
                              html_names::kSelectTag)) {
    return true;
  }
  if (IsForMarkupSanitization()) {
    // During sanitization, iframes in the staging document haven't loaded and
    // are hence not rendered. They should still be serialized.
    if (IsA<HTMLIFrameElement>(node))
      return true;
  }
  return false;
}

template class StyledMarkupSerializer<EditingStrategy>;
template class StyledMarkupSerializer<EditingInFlatTreeStrategy>;

}  // namespace blink
