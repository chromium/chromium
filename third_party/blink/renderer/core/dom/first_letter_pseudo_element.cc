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
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// CSS 2.1 http://www.w3.org/TR/CSS21/selector.html#first-letter "Punctuation
// (i.e, characters defined in Unicode [UNICODE] in the "open" (Ps), "close"
// (Pe), "initial" (Pi). "final" (Pf) and "other" (Po) punctuation classes),
// that precedes or follows the first letter should be included"
static inline bool IsPunctuationForFirstLetter(UChar32 c) {
  WTF::unicode::CharCategory char_category = WTF::unicode::Category(c);
  return char_category == WTF::unicode::kPunctuation_Open ||
         char_category == WTF::unicode::kPunctuation_Close ||
         char_category == WTF::unicode::kPunctuation_InitialQuote ||
         char_category == WTF::unicode::kPunctuation_FinalQuote ||
         char_category == WTF::unicode::kPunctuation_Other;
}

static inline bool IsSpaceForFirstLetter(UChar c) {
  return IsSpaceOrNewline(c) || c == WTF::unicode::kNoBreakSpaceCharacter;
}

unsigned FirstLetterPseudoElement::FirstLetterLength(const String& text) {
  unsigned length = 0;
  unsigned text_length = text.length();

  if (text_length == 0)
    return length;

  // Account for leading spaces first.
  while (length < text_length && IsSpaceForFirstLetter(text[length]))
    length++;
  // Now account for leading punctuation.
  while (length < text_length &&
         IsPunctuationForFirstLetter(text.CharacterStartingAt(length)))
    length += LengthOfGraphemeCluster(text, length);

  // Bail if we didn't find a letter before the end of the text or before a
  // space.
  if (IsSpaceForFirstLetter(text[length]) || length == text_length)
    return 0;

  // Account the next character for first letter.
  length += LengthOfGraphemeCluster(text, length);

  // Keep looking for allowed punctuation for the :first-letter.
  unsigned num_code_units = 0;
  for (; length < text_length; length += num_code_units) {
    UChar32 c = text.CharacterStartingAt(length);
    if (!IsPunctuationForFirstLetter(c))
      break;
    num_code_units = LengthOfGraphemeCluster(text, length);
  }
  return length;
}

void FirstLetterPseudoElement::Trace(Visitor* visitor) const {
  visitor->Trace(remaining_text_layout_object_);
  PseudoElement::Trace(visitor);
}

// Once we see any of these layoutObjects we can stop looking for first-letter
// as they signal the end of the first line of text.
static bool IsInvalidFirstLetterLayoutObject(const LayoutObject* obj) {
  return (obj->IsBR() || (obj->IsText() && To<LayoutText>(obj)->IsWordBreak()));
}

static bool IsParentInlineLayoutObject(const LayoutObject* obj) {
  return (obj && obj->Parent() && obj->Parent()->IsLayoutInline());
}

LayoutText* FirstLetterPseudoElement::FirstLetterTextLayoutObject(
    const Element& element) {
  LayoutObject* parent_layout_object = nullptr;

  // If we are looking at a first letter element then we need to find the
  // first letter text LayoutObject from the parent node, and not ourselves.
  if (element.IsFirstLetterPseudoElement()) {
    parent_layout_object =
        element.ParentOrShadowHostElement()->GetLayoutObject();
  } else {
    parent_layout_object = element.GetLayoutObject();
  }

  if (!parent_layout_object ||
      !parent_layout_object->Style()->HasPseudoElementStyle(
          kPseudoIdFirstLetter) ||
      !CanHaveGeneratedChildren(*parent_layout_object) ||
      !parent_layout_object->BehavesLikeBlockContainer())
    return nullptr;

  // Drill down into our children and look for our first text child.
  LayoutObject* first_letter_text_layout_object =
      parent_layout_object->SlowFirstChild();
  while (first_letter_text_layout_object) {
    // This can be called when the first letter layoutObject is already in the
    // tree. We do not want to consider that layoutObject for our text
    // layoutObject so we go to the sibling (which is the LayoutTextFragment for
    // the remaining text).
    if (first_letter_text_layout_object->Style() &&
        first_letter_text_layout_object->Style()->StyleType() ==
            kPseudoIdFirstLetter) {
      first_letter_text_layout_object =
          first_letter_text_layout_object->NextSibling();
    } else if (auto* layout_text =
                   DynamicTo<LayoutText>(first_letter_text_layout_object)) {
      // Don't apply first letter styling to passwords and other elements
      // obfuscated by -webkit-text-security. Also, see
      // ShouldUpdateLayoutByReattaching() in text.cc.
      if (layout_text->IsSecure())
        return nullptr;
      // FIXME: If there is leading punctuation in a different LayoutText than
      // the first letter, we'll not apply the correct style to it.
      String str = layout_text->IsTextFragment()
                       ? To<LayoutTextFragment>(first_letter_text_layout_object)
                             ->CompleteText()
                       : layout_text->OriginalText();
      if (FirstLetterLength(str.Impl()) ||
          IsInvalidFirstLetterLayoutObject(first_letter_text_layout_object)) {
        break;
      }

      // In case of inline level content made of punctuation and there is no
      // sibling, we'll apply style to it.
      if (IsParentInlineLayoutObject(first_letter_text_layout_object) &&
          str.length() && !first_letter_text_layout_object->NextSibling()) {
        break;
      }

      first_letter_text_layout_object =
          first_letter_text_layout_object->NextSibling();
    } else if (first_letter_text_layout_object->IsListMarker()) {
      // The list item marker may have out-of-flow siblings inside an anonymous
      // block. Skip them to make sure we leave the anonymous block before
      // continuing looking for the first letter text.
      do {
        first_letter_text_layout_object =
            first_letter_text_layout_object->NextInPreOrderAfterChildren(
                parent_layout_object);
      } while (
          first_letter_text_layout_object &&
          first_letter_text_layout_object->IsFloatingOrOutOfFlowPositioned());
    } else if (first_letter_text_layout_object
                   ->IsFloatingOrOutOfFlowPositioned()) {
      if (first_letter_text_layout_object->Style()->StyleType() ==
          kPseudoIdFirstLetter) {
        first_letter_text_layout_object =
            first_letter_text_layout_object->SlowFirstChild();
        break;
      }
      first_letter_text_layout_object =
          first_letter_text_layout_object->NextSibling();
    } else if (first_letter_text_layout_object->IsAtomicInlineLevel() ||
               first_letter_text_layout_object->IsButton() ||
               IsMenuList(first_letter_text_layout_object)) {
      return nullptr;
    } else if (first_letter_text_layout_object->IsFlexibleBoxIncludingNG() ||
               first_letter_text_layout_object->IsLayoutNGGrid()) {
      first_letter_text_layout_object =
          first_letter_text_layout_object->NextSibling();
    } else if (!first_letter_text_layout_object->IsInline() &&
               first_letter_text_layout_object->Style()->HasPseudoElementStyle(
                   kPseudoIdFirstLetter) &&
               CanHaveGeneratedChildren(*first_letter_text_layout_object)) {
      // There is a layoutObject further down the tree which has
      // PseudoIdFirstLetter set. When that node is attached we will handle
      // setting up the first letter then.
      return nullptr;
    } else if ((first_letter_text_layout_object->IsInline() ||
                first_letter_text_layout_object->IsAnonymousBlock()) &&
               !first_letter_text_layout_object->SlowFirstChild()) {
      if (LayoutObject* next_sibling =
              first_letter_text_layout_object->NextSibling()) {
        first_letter_text_layout_object = next_sibling;
        continue;
      }
      LayoutObject* parent = first_letter_text_layout_object->Parent();
      if (parent && parent != parent_layout_object) {
        first_letter_text_layout_object = parent->NextSibling();
        continue;
      }
      return nullptr;
    } else {
      first_letter_text_layout_object =
          first_letter_text_layout_object->SlowFirstChild();
    }
  }

  // No first letter text to display, we're done.
  // FIXME: This list of disallowed LayoutText subclasses is fragile.
  // crbug.com/422336.
  // Should counter be on this list? What about LayoutTextFragment?
  if (!first_letter_text_layout_object ||
      !first_letter_text_layout_object->IsText() ||
      IsInvalidFirstLetterLayoutObject(first_letter_text_layout_object))
    return nullptr;

  return To<LayoutText>(first_letter_text_layout_object);
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

  unsigned length = FirstLetterPseudoElement::FirstLetterLength(old_text);
  remaining_text_layout_object_->SetTextFragment(
      old_text.Impl()->Substring(length, old_text.length()), length,
      old_text.length() - length);
  remaining_text_layout_object_->DirtyLineBoxes();

  for (auto* child = GetLayoutObject()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsText() || !To<LayoutText>(child)->IsTextFragment())
      continue;
    auto* child_fragment = To<LayoutTextFragment>(child);
    if (child_fragment->GetFirstLetterPseudoElement() != this)
      continue;

    child_fragment->SetTextFragment(old_text.Impl()->Substring(0, length), 0,
                                    length);
    child_fragment->DirtyLineBoxes();

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
  if (UNLIKELY(!style.InitialLetter().IsNormal())) {
    return LayoutObject::CreateBlockFlowOrListItem(this, style);
  }

  return PseudoElement::CreateLayoutObject(style);
}

scoped_refptr<const ComputedStyle>
FirstLetterPseudoElement::CustomStyleForLayoutObject(
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
  unsigned length = FirstLetterPseudoElement::FirstLetterLength(old_text);

  // In case of inline level content made of punctuation, we use
  // first_letter_text length instead of FirstLetterLength.
  if (IsParentInlineLayoutObject(first_letter_text) && length == 0 &&
      first_letter_text->TextLength())
    length = first_letter_text->TextLength();

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
        *this, old_text.Impl(), length, remaining_length);
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
  LayoutTextFragment* letter =
      LayoutTextFragment::CreateAnonymous(*this, old_text.Impl(), 0, length);
  letter->SetFirstLetterPseudoElement(this);
  if (UNLIKELY(GetLayoutObject()->IsInitialLetterBox())) {
    const LayoutBlock& paragraph = *GetLayoutObject()->ContainingBlock();
    // TODO(crbug.com/1393280): Once we can store used font somewhere, we should
    // compute initial-letter font during layout to take proper effective style.
    const ComputedStyle& paragraph_style =
        paragraph.EffectiveStyle(NGStyleVariant::kFirstLine);
    scoped_refptr<const ComputedStyle> initial_letter_text_style =
        GetDocument().GetStyleResolver().StyleForInitialLetterText(
            *letter_style, paragraph_style);
    letter->SetStyle(std::move(initial_letter_text_style));
  } else {
    letter->SetStyle(letter_style);
  }
  GetLayoutObject()->AddChild(letter);

  first_letter_text->Destroy();
}

Node* FirstLetterPseudoElement::InnerNodeForHitTesting() const {
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
