/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/generated_children.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// CSS 2.1 http://www.w3.org/TR/CSS21/selector.html#first-letter "Punctuation
// (i.e, characters defined in Unicode [UNICODE] in the "open" (Ps), "close"
// (Pe), "initial" (Pi). "final" (Pf) and "other" (Po) punctuation classes),
// that precedes or follows the first letter should be included"
inline bool IsPunctuationForFirstLetter(UChar32 c) {
  WTF::unicode::CharCategory char_category = WTF::unicode::Category(c);
  return char_category == WTF::unicode::kPunctuation_Open ||
         char_category == WTF::unicode::kPunctuation_Close ||
         char_category == WTF::unicode::kPunctuation_InitialQuote ||
         char_category == WTF::unicode::kPunctuation_FinalQuote ||
         char_category == WTF::unicode::kPunctuation_Other;
}

bool IsPunctuationForFirstLetter(const String& string, unsigned offset) {
  return IsPunctuationForFirstLetter(*StringView(string, offset).begin());
}

inline bool IsNewLine(UChar c) {
  if (c == 0xA || c == 0xD) {
    return true;
  }

  return false;
}

inline bool IsSpace(UChar c) {
  if (IsNewLine(c)) {
    return false;
  }

  return IsSpaceOrNewline(c);
}

inline bool IsSpaceForFirstLetter(UChar c, bool preserve_breaks) {
  return (preserve_breaks ? IsSpace(c) : IsSpaceOrNewline(c)) ||
         c == WTF::unicode::kNoBreakSpaceCharacter;
}

bool IsParentInlineLayoutObject(const LayoutObject* layout_object) {
  return layout_object && IsA<LayoutInline>(layout_object->Parent());
}

}  // namespace

unsigned FirstLetterPseudoElement::FirstLetterLength(const String& text,
                                                     bool preserve_breaks,
                                                     Punctuation& punctuation) {
  DCHECK_NE(punctuation, Punctuation::kDisallow);

  unsigned length = 0;
  unsigned text_length = text.length();

  if (text_length == 0) {
    return length;
  }

  // Account for leading spaces first. If there is leading punctuation from a
  // different text node, spaces can not appear in between to form valid
  // ::first-letter text.
  if (punctuation == Punctuation::kNotSeen) {
    while (length < text_length &&
           IsSpaceForFirstLetter(text[length], preserve_breaks)) {
      length++;
    }
    if (length == text_length) {
      // Only contains spaces.
      return 0;
    }
  }

  unsigned punctuation_start = length;
  // Now account for leading punctuation.
  while (length < text_length && IsPunctuationForFirstLetter(text, length)) {
    length += LengthOfGraphemeCluster(text, length);
  }

  if (length == text_length) {
    if (length > punctuation_start) {
      // Text ends at allowed leading punctuation. Signal that we may continue
      // looking for ::first-letter text in the next text node, including more
      // punctuation.
      punctuation = Punctuation::kSeen;
      return length;
    }
  }

  // Stop allowing leading punctuation.
  punctuation = Punctuation::kDisallow;

  DCHECK_LT(length, text_length);
  if (IsSpaceForFirstLetter(text[length], preserve_breaks) ||
      IsNewLine(text[length])) {
    return 0;
  }

  // Account the next character for first letter.
  length += LengthOfGraphemeCluster(text, length);

  // Keep looking for allowed punctuation for the ::first-letter within the same
  // text node. We are allowed to ignore trailing punctuation in following text
  // nodes per spec.
  unsigned num_code_units = 0;
  for (; length < text_length; length += num_code_units) {
    if (!IsPunctuationForFirstLetter(text, length)) {
      break;
    }
    num_code_units = LengthOfGraphemeCluster(text, length);
  }
  return length;
}

void FirstLetterPseudoElement::Trace(Visitor* visitor) const {
  visitor->Trace(remaining_text_layout_object_);
  PseudoElement::Trace(visitor);
}

namespace {

LayoutObject* FirstInFlowInlineDescendantForFirstLetter(LayoutObject& parent) {
  // https://drafts.csswg.org/css-pseudo/#first-text-line:
  //
  // - The first formatted line of a block container that establishes an inline
  //   formatting context represents the inline-level content of its first line
  //   box.
  // - The first formatted line of a block container or multi-column container
  //   that contains block-level content (and is not a table wrapper box) is the
  //   first formatted line of its first in-flow block-level child. If no such
  //   line exists, it has no first formatted line.
  LayoutObject* first_inline = parent.SlowFirstChild();

  while (first_inline) {
    if (first_inline->IsFloatingOrOutOfFlowPositioned()) {
      first_inline = first_inline->NextSibling();
      continue;
    }
    if (first_inline->IsListMarker()) {
      LayoutObject* list_item = first_inline;
      while (list_item && !list_item->IsLayoutListItem()) {
        DCHECK_NE(list_item, &parent);
        list_item = list_item->Parent();
      }
      // Skip the marker contents, but don't escape the list item.
      first_inline = first_inline->NextInPreOrderAfterChildren(list_item);
      continue;
    }
    if (first_inline->IsInline()) {
      return first_inline;
    }
    if (!first_inline->BehavesLikeBlockContainer()) {
      // Block level in-flow displays like flex, grid, and table do not have a
      // first formatted line.
      return nullptr;
    }
    if (first_inline->IsButtonOrInputButton()) {
      // Buttons do not accept the first-letter.
      return nullptr;
    }
    if (first_inline->StyleRef().HasPseudoElementStyle(kPseudoIdFirstLetter)) {
      // Applying ::first-letter styles from multiple nested containers is not
      // supported. ::first-letter styles from the inner-most container is
      // applied - bail out.
      return nullptr;
    }
    first_inline = first_inline->SlowFirstChild();
  }
  return nullptr;
}

}  // namespace

LayoutText* FirstLetterPseudoElement::FirstLetterTextLayoutObject(
    const Element& element) {
  LayoutObject* parent_layout_object = nullptr;

  if (element.IsFirstLetterPseudoElement()) {
    // If the passed-in element is a ::first-letter pseudo element we need to
    // start from the originating element.
    parent_layout_object =
        element.ParentOrShadowHostElement()->GetLayoutObject();
  } else {
    parent_layout_object = element.GetLayoutObject();
  }

  if (!parent_layout_object ||
      !parent_layout_object->StyleRef().HasPseudoElementStyle(
          kPseudoIdFirstLetter) ||
      !CanHaveGeneratedChildren(*parent_layout_object) ||
      !parent_layout_object->BehavesLikeBlockContainer()) {
    // This element can not have a styleable ::first-letter.
    return nullptr;
  }

  LayoutObject* inline_child =
      FirstInFlowInlineDescendantForFirstLetter(*parent_layout_object);
  if (!inline_child) {
    return nullptr;
  }

  LayoutObject* stay_inside = inline_child->Parent();
  LayoutText* punctuation_text = nullptr;
  Punctuation punctuation = Punctuation::kNotSeen;

  while (inline_child) {
    if (inline_child->StyleRef().StyleType() == kPseudoIdFirstLetter) {
      // This can be called when the ::first-letter LayoutObject is already in
      // the tree. We do not want to consider that LayoutObject for our text
      // LayoutObject so we go to the sibling (which is the LayoutTextFragment
      // for the remaining text).
      inline_child = inline_child->NextSibling();
    } else if (inline_child->IsListMarker()) {
      inline_child = inline_child->NextInPreOrderAfterChildren(stay_inside);
    } else if (inline_child->IsInline()) {
      if (auto* layout_text = DynamicTo<LayoutText>(inline_child)) {
        // Don't apply first letter styling to passwords and other elements
        // obfuscated by -webkit-text-security. Also, see
        // ShouldUpdateLayoutByReattaching() in text.cc.
        if (layout_text->IsSecure()) {
          return nullptr;
        }
        if (layout_text->IsBR() || layout_text->IsWordBreak()) {
          return nullptr;
        }
        String str = layout_text->IsTextFragment()
                         ? To<LayoutTextFragment>(inline_child)->CompleteText()
                         : layout_text->OriginalText();
        bool preserve_breaks = ShouldPreserveBreaks(
            inline_child->StyleRef().GetWhiteSpaceCollapse());

        if (FirstLetterLength(str, preserve_breaks, punctuation)) {
          // A prefix, or the whole text for the current layout_text is
          // included in the valid ::first-letter text.

          if (punctuation == Punctuation::kSeen) {
            // So far, we have only seen punctuation. Need to continue looking
            // for a typographic character unit to go along with the
            // punctuation.
            if (!punctuation_text) {
              punctuation_text = layout_text;
            }
          } else {
            // We have found valid ::first-letter text. When the ::first-letter
            // text spans multiple elements, the UA is free to style only one of
            // the elements, all of the elements, or none of the elements. Here
            // we choose to return the first, which matches the Firefox
            // behavior.
            if (punctuation_text) {
              return punctuation_text;
            } else {
              return layout_text;
            }
          }
        } else if (punctuation == Punctuation::kDisallow) {
          // No ::first-letter text seen in this text node. Non-null
          // punctuation_text means we have seen punctuation in a previous text
          // node, but leading_punctuation was reset to false as we encountered
          // spaces or other content that is neither punctuation nor a valid
          // typographic character unit for ::first-letter.
          return nullptr;
        }
      } else if (inline_child->IsAtomicInlineLevel() ||
                 inline_child->IsMenuList()) {
        return nullptr;
      }
      inline_child = inline_child->NextInPreOrder(stay_inside);
    } else if (inline_child->IsFloatingOrOutOfFlowPositioned()) {
      if (inline_child->StyleRef().StyleType() == kPseudoIdFirstLetter) {
        inline_child = inline_child->SlowFirstChild();
      } else {
        inline_child = inline_child->NextInPreOrderAfterChildren(stay_inside);
      }
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

FirstLetterPseudoElement::FirstLetterPseudoElement(Element* parent)
    : PseudoElement(parent, kPseudoIdFirstLetter),
      remaining_text_layout_object_(nullptr) {}

FirstLetterPseudoElement::~FirstLetterPseudoElement() {
  DCHECK(!remaining_text_layout_object_);
}

void FirstLetterPseudoElement::UpdateTextFragments() {
  String old_text(remaining_text_layout_object_->CompleteText());
  DCHECK(old_text.Impl());

  bool preserve_breaks = ShouldPreserveBreaks(
      remaining_text_layout_object_->StyleRef().GetWhiteSpaceCollapse());
  FirstLetterPseudoElement::Punctuation punctuation =
      FirstLetterPseudoElement::Punctuation::kNotSeen;
  unsigned length = FirstLetterPseudoElement::FirstLetterLength(
      old_text, preserve_breaks, punctuation);
  remaining_text_layout_object_->SetTextFragment(
      old_text.Impl()->Substring(length, old_text.length()), length,
      old_text.length() - length);
  remaining_text_layout_object_->InvalidateInlineItems();

  for (auto* child = GetLayoutObject()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsText() || !To<LayoutText>(child)->IsTextFragment())
      continue;
    auto* child_fragment = To<LayoutTextFragment>(child);
    if (child_fragment->GetFirstLetterPseudoElement() != this)
      continue;

    child_fragment->SetTextFragment(old_text.Impl()->Substring(0, length), 0,
                                    length);
    child_fragment->InvalidateInlineItems();

    // Make sure the first-letter layoutObject is set to require a layout as it
    // needs to re-create the line boxes. The remaining text layoutObject
    // will be marked by the LayoutText::setText.
    child_fragment->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kTextChanged);
    break;
  }
}

void FirstLetterPseudoElement::ClearRemainingTextLayoutObject() {
  DCHECK(remaining_text_layout_object_);
  remaining_text_layout_object_ = nullptr;

  if (GetDocument().InStyleRecalc()) {
    // UpdateFirstLetterPseudoElement will handle remaining_text_layout_object_
    // changes during style recalc and layout tree rebuild.
    return;
  }

  // When we remove nodes from the tree, we do not mark ancestry for
  // ChildNeedsStyleRecalc(). When removing the text node which contains the
  // first letter, we need to UpdateFirstLetter to render the new first letter
  // or remove the ::first-letter pseudo if there is no text left. Do that as
  // part of a style recalc for this ::first-letter.
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kPseudoClass));
}

void FirstLetterPseudoElement::AttachLayoutTree(AttachContext& context) {
  LayoutText* first_letter_text =
      FirstLetterPseudoElement::FirstLetterTextLayoutObject(*this);
  // The FirstLetterPseudoElement should have been removed in
  // Element::UpdateFirstLetterPseudoElement(). However if there existed a first
  // letter before updating it, the layout tree will be different after
  // DetachLayoutTree() called right before this method.
  // If there is a bug in FirstLetterTextLayoutObject(), we might end up with
  // null here. DCHECKing here, but handling the null pointer below to avoid
  // crashes.
  DCHECK(first_letter_text);

  AttachContext first_letter_context(context);
  first_letter_context.next_sibling = first_letter_text;
  first_letter_context.next_sibling_valid = true;
  if (first_letter_text) {
    first_letter_context.parent = first_letter_text->Parent();
  }
  PseudoElement::AttachLayoutTree(first_letter_context);
  if (first_letter_text)
    AttachFirstLetterTextLayoutObjects(first_letter_text);
}

void FirstLetterPseudoElement::DetachLayoutTree(bool performing_reattach) {
  if (remaining_text_layout_object_) {
    if (remaining_text_layout_object_->GetNode() && GetDocument().IsActive()) {
      auto* text_node = To<Text>(remaining_text_layout_object_->GetNode());
      remaining_text_layout_object_->SetTextFragment(
          text_node->data(), 0, text_node->data().length());
    }
    remaining_text_layout_object_->SetFirstLetterPseudoElement(nullptr);
    remaining_text_layout_object_->SetIsRemainingTextLayoutObject(false);
  }
  remaining_text_layout_object_ = nullptr;

  PseudoElement::DetachLayoutTree(performing_reattach);
}

LayoutObject* FirstLetterPseudoElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (!style.InitialLetter().IsNormal()) [[unlikely]] {
    return LayoutObject::CreateBlockFlowOrListItem(this, style);
  }

  return PseudoElement::CreateLayoutObject(style);
}

const ComputedStyle* FirstLetterPseudoElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  LayoutObject* first_letter_text =
      FirstLetterPseudoElement::FirstLetterTextLayoutObject(*this);
  if (!first_letter_text)
    return nullptr;
  DCHECK(first_letter_text->Parent());
  return ParentOrShadowHostElement()->StyleForPseudoElement(
      style_recalc_context,
      StyleRequest(GetPseudoId(),
                   first_letter_text->Parent()->FirstLineStyle()));
}

void FirstLetterPseudoElement::AttachFirstLetterTextLayoutObjects(
    LayoutText* first_letter_text) {
  DCHECK(first_letter_text);

  // The original string is going to be either a generated content string or a
  // DOM node's string. We want the original string before it got transformed in
  // case first-letter has no text-transform or a different text-transform
  // applied to it.
  String old_text =
      first_letter_text->IsTextFragment()
          ? To<LayoutTextFragment>(first_letter_text)->CompleteText()
          : first_letter_text->OriginalText();
  DCHECK(old_text.Impl());

  // FIXME: This would already have been calculated in firstLetterLayoutObject.
  // Can we pass the length through?
  bool preserve_breaks = ShouldPreserveBreaks(
      first_letter_text->StyleRef().GetWhiteSpaceCollapse());
  FirstLetterPseudoElement::Punctuation punctuation =
      FirstLetterPseudoElement::Punctuation::kNotSeen;
  unsigned length = FirstLetterPseudoElement::FirstLetterLength(
      old_text, preserve_breaks, punctuation);

  // In case of inline level content made of punctuation, we use
  // the whole text length instead of FirstLetterLength.
  if (IsParentInlineLayoutObject(first_letter_text) && length == 0 &&
      old_text.length()) {
    length = old_text.length();
  }

  unsigned remaining_length = old_text.length() - length;

  // Construct a text fragment for the text after the first letter.
  // This text fragment might be empty.
  LayoutTextFragment* remaining_text;

  if (first_letter_text->GetNode()) {
    remaining_text =
        LayoutTextFragment::Create(first_letter_text->GetNode(),
                                   old_text.Impl(), length, remaining_length);
  } else {
    remaining_text = LayoutTextFragment::CreateAnonymous(
        GetDocument(), old_text.Impl(), length, remaining_length);
  }

  remaining_text->SetFirstLetterPseudoElement(this);
  remaining_text->SetIsRemainingTextLayoutObject(true);
  remaining_text->SetStyle(first_letter_text->Style());

  if (remaining_text->GetNode())
    remaining_text->GetNode()->SetLayoutObject(remaining_text);

  remaining_text_layout_object_ = remaining_text;

  LayoutObject* next_sibling = GetLayoutObject()->NextSibling();
  GetLayoutObject()->Parent()->AddChild(remaining_text, next_sibling);

  // Construct text fragment for the first letter.
  const ComputedStyle* const letter_style = GetComputedStyle();
  LayoutTextFragment* letter = LayoutTextFragment::CreateAnonymous(
      GetDocument(), old_text.Impl(), 0, length);
  letter->SetFirstLetterPseudoElement(this);
  if (GetLayoutObject()->IsInitialLetterBox()) [[unlikely]] {
    const LayoutBlock& paragraph = *GetLayoutObject()->ContainingBlock();
    // TODO(crbug.com/1393280): Once we can store used font somewhere, we should
    // compute initial-letter font during layout to take proper effective style.
    const ComputedStyle& paragraph_style =
        paragraph.EffectiveStyle(StyleVariant::kFirstLine);
    const ComputedStyle* initial_letter_text_style =
        GetDocument().GetStyleResolver().StyleForInitialLetterText(
            *letter_style, paragraph_style);
    letter->SetStyle(std::move(initial_letter_text_style));
  } else {
    letter->SetStyle(letter_style);
  }
  GetLayoutObject()->AddChild(letter);

  // AXObjects are normally removed from destroyed layout objects in
  // Node::DetachLayoutTree(), but as the ::first-letter implementation manually
  // destroys the layout object for the first letter text, it must manually
  // remove the accessibility object for it as well.
  if (auto* cache = GetDocument().ExistingAXObjectCache()) {
    cache->RemoveAXObjectsInLayoutSubtree(first_letter_text);
  }
  first_letter_text->Destroy();
}

Node* FirstLetterPseudoElement::InnerNodeForHitTesting() {
  // When we hit a first letter during hit testing, hover state and events
  // should be triggered on the parent of the real text node where the first
  // letter is taken from. The first letter may not come from a real node - for
  // quotes and generated text in ::before/::after. In that case walk up the
  // layout tree to find the closest ancestor which is not anonymous. Note that
  // display:contents will not be skipped since we generate anonymous
  // LayoutInline boxes for ::before/::after with display:contents.
  DCHECK(remaining_text_layout_object_);
  LayoutObject* layout_object = remaining_text_layout_object_;
  while (layout_object->IsAnonymous()) {
    layout_object = layout_object->Parent();
    DCHECK(layout_object);
  }
  Node* node = layout_object->GetNode();
  DCHECK(node);
  if (layout_object == remaining_text_layout_object_) {
    // The text containing the first-letter is a real node, return its flat tree
    // parent. If we used the layout tree parent, we would have incorrectly
    // skipped display:contents ancestors.
    return FlatTreeTraversal::Parent(*node);
  }
  if (node->IsPseudoElement()) {
    // ::first-letter in generated content for ::before/::after. Use pseudo
    // element parent.
    return node->ParentOrShadowHostNode();
  }
  return node;
}

}  // namespace blink
