/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

LayoutTextFragment::LayoutTextFragment(Node* node,
                                       StringImpl* str,
                                       int start_offset,
                                       int length)
    : LayoutText(node, str ? str->Substring(start_offset, length) : nullptr),
      start_(start_offset),
      fragment_length_(length),
      is_remaining_text_layout_object_(false),
      content_string_(str),
      first_letter_pseudo_element_(nullptr) {
  is_text_fragment_ = true;
}

LayoutTextFragment::~LayoutTextFragment() {
  DCHECK(!first_letter_pseudo_element_);
}

LayoutTextFragment* LayoutTextFragment::Create(Node* node,
                                               StringImpl* str,
                                               int start_offset,
                                               int length,
                                               LegacyLayout legacy) {
  return LayoutObjectFactory::CreateTextFragment(node, str, start_offset,
                                                 length, legacy);
}

LayoutTextFragment* LayoutTextFragment::CreateAnonymous(Document& doc,
                                                        StringImpl* text,
                                                        unsigned start,
                                                        unsigned length,
                                                        LegacyLayout legacy) {
  LayoutTextFragment* fragment =
      LayoutTextFragment::Create(nullptr, text, start, length, legacy);
  fragment->SetDocumentForAnonymous(&doc);
  if (length)
    doc.View()->IncrementVisuallyNonEmptyCharacterCount(length);
  return fragment;
}

LayoutTextFragment* LayoutTextFragment::CreateAnonymous(PseudoElement& pseudo,
                                                        StringImpl* text,
                                                        unsigned start,
                                                        unsigned length,
                                                        LegacyLayout legacy) {
  return CreateAnonymous(pseudo.GetDocument(), text, start, length, legacy);
}

LayoutTextFragment* LayoutTextFragment::CreateAnonymous(PseudoElement& pseudo,
                                                        StringImpl* text,
                                                        LegacyLayout legacy) {
  return CreateAnonymous(pseudo, text, 0, text ? text->length() : 0, legacy);
}

void LayoutTextFragment::Trace(Visitor* visitor) const {
  visitor->Trace(first_letter_pseudo_element_);
  LayoutText::Trace(visitor);
}

void LayoutTextFragment::WillBeDestroyed() {
  NOT_DESTROYED();
  if (is_remaining_text_layout_object_ && first_letter_pseudo_element_)
    first_letter_pseudo_element_->ClearRemainingTextLayoutObject();
  first_letter_pseudo_element_ = nullptr;
  LayoutText::WillBeDestroyed();
}

scoped_refptr<StringImpl> LayoutTextFragment::CompleteText() const {
  NOT_DESTROYED();
  Text* text = AssociatedTextNode();
  return text ? text->DataImpl() : ContentString();
}

void LayoutTextFragment::SetContentString(StringImpl* str) {
  NOT_DESTROYED();
  content_string_ = str;
  SetTextIfNeeded(str);
}

scoped_refptr<StringImpl> LayoutTextFragment::OriginalText() const {
  NOT_DESTROYED();
  scoped_refptr<StringImpl> result = CompleteText();
  if (!result)
    return nullptr;
  return result->Substring(Start(), FragmentLength());
}

void LayoutTextFragment::TextDidChange() {
  NOT_DESTROYED();
  LayoutText::TextDidChange();

  start_ = 0;
  fragment_length_ = TextLength();

  // If we're the remaining text from a first letter then we have to tell the
  // first letter pseudo element to reattach itself so it can re-calculate the
  // correct first-letter settings.
  if (IsRemainingTextLayoutObject()) {
    DCHECK(GetFirstLetterPseudoElement());
    GetFirstLetterPseudoElement()->UpdateTextFragments();
  }
}

// Unlike |ForceSetText()|, this function is used for updating first-letter part
// or remaining part.
void LayoutTextFragment::SetTextFragment(scoped_refptr<StringImpl> text,
                                         unsigned start,
                                         unsigned length) {
  NOT_DESTROYED();
  // Note, we have to call |LayoutText::TextDidChange()| here because, if we
  // use our version we will, potentially, screw up the first-letter settings
  // where we only use portions of the string.
  if (!Equal(GetText().Impl(), text.get())) {
    SetTextInternal(std::move(text));
    LayoutText::TextDidChange();
  }

  start_ = start;
  fragment_length_ = length;
}

void LayoutTextFragment::TransformText() {
  NOT_DESTROYED();
  // Note, we have to call LayoutText::TextDidChange()| here because, if we use
  // our version we will, potentially, screw up the first-letter settings where
  // we only use portions of the string.
  if (scoped_refptr<StringImpl> text_to_transform = OriginalText()) {
    SetTextInternal(std::move(text_to_transform));
    LayoutText::TextDidChange();
  }
}

UChar LayoutTextFragment::PreviousCharacter() const {
  NOT_DESTROYED();
  if (Start()) {
    StringImpl* original = CompleteText().get();
    if (original && Start() <= original->length())
      return (*original)[Start() - 1];
  }

  return LayoutText::PreviousCharacter();
}

// If this is the layoutObject for a first-letter pseudoNode then we have to
// look at the node for the remaining text to find our content.
Text* LayoutTextFragment::AssociatedTextNode() const {
  NOT_DESTROYED();
  Node* node = GetFirstLetterPseudoElement();
  if (is_remaining_text_layout_object_ || !node) {
    // If we don't have a node, then we aren't part of a first-letter pseudo
    // element, so use the actual node. Likewise, if we have a node, but
    // we're the remainingTextLayoutObject for a pseudo element use the real
    // text node.
    node = GetNode();
  }

  if (!node)
    return nullptr;

  if (auto* pseudo = DynamicTo<FirstLetterPseudoElement>(node)) {
    LayoutObject* next_layout_object =
        FirstLetterPseudoElement::FirstLetterTextLayoutObject(*pseudo);
    if (!next_layout_object)
      return nullptr;
    node = next_layout_object->GetNode();
  }
  return DynamicTo<Text>(node);
}

LayoutText* LayoutTextFragment::GetFirstLetterPart() const {
  NOT_DESTROYED();
  if (!is_remaining_text_layout_object_)
    return nullptr;
  LayoutObject* const first_letter_container =
      GetFirstLetterPseudoElement()->GetLayoutObject();
  LayoutObject* child = first_letter_container->SlowFirstChild();
  if (!child->IsText()) {
    DCHECK(!IsInLayoutNGInlineFormattingContext());
    // In legacy layout there may also be a list item marker here. The next
    // sibling better be the LayoutTextFragment of the ::first-letter, then.
    child = child->NextSibling();
    DCHECK(child);
  }
  CHECK(child->IsText());
  DCHECK_EQ(child, first_letter_container->SlowLastChild());
  return To<LayoutTextFragment>(child);
}

void LayoutTextFragment::UpdateHitTestResult(
    HitTestResult& result,
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  if (result.InnerNode())
    return;

  LayoutObject::UpdateHitTestResult(result, point);

  // If we aren't part of a first-letter element, or if we
  // are part of first-letter but we're the remaining text then return.
  if (is_remaining_text_layout_object_ || !GetFirstLetterPseudoElement())
    return;
  result.SetInnerNode(GetFirstLetterPseudoElement());
}

DOMNodeId LayoutTextFragment::OwnerNodeId() const {
  NOT_DESTROYED();
  Node* node = AssociatedTextNode();
  return node ? DOMNodeIds::IdForNode(node) : kInvalidDOMNodeId;
}

Position LayoutTextFragment::PositionForCaretOffset(unsigned offset) const {
  NOT_DESTROYED();
  // TODO(layout-dev): Make the following DCHECK always enabled after we
  // properly support 'text-transform' changing text length.
#if DCHECK_IS_ON()
  if (StyleRef().TextTransform() == ETextTransform::kNone)
    DCHECK_LE(offset, FragmentLength());
#endif
  const Text* node = AssociatedTextNode();
  if (!node)
    return Position();
  // TODO(layout-dev): Properly support offset change due to text-transform.
  const unsigned clamped_offset = std::min(offset, FragmentLength());
  return Position(node, Start() + clamped_offset);
}

absl::optional<unsigned> LayoutTextFragment::CaretOffsetForPosition(
    const Position& position) const {
  NOT_DESTROYED();
  if (position.IsNull() || position.AnchorNode() != AssociatedTextNode())
    return absl::nullopt;
  unsigned dom_offset;
  if (position.IsBeforeAnchor()) {
    dom_offset = 0;
  } else if (position.IsAfterAnchor()) {
    // TODO(layout-dev): Support offset change due to text-transform.
    dom_offset = Start() + FragmentLength();
  } else {
    DCHECK(position.IsOffsetInAnchor()) << position;
    // TODO(layout-dev): Support offset change due to text-transform.
    dom_offset = position.OffsetInContainerNode();
  }
  if (dom_offset < Start() || dom_offset > Start() + FragmentLength())
    return absl::nullopt;
  return dom_offset - Start();
}

String LayoutTextFragment::PlainText() const {
  // Special handling for floating ::first-letter in LayoutNG to ensure that
  // PlainText() returns the full text of the node, not just the remaining text.
  // See also ElementInnerTextCollector::ProcessTextNode(), which does the same.
  NOT_DESTROYED();
  if (!is_remaining_text_layout_object_ || !GetNode())
    return LayoutText::PlainText();
  LayoutText* first_letter = GetFirstLetterPart();
  if (!first_letter)
    return LayoutText::PlainText();
  const NGOffsetMapping* remaining_text_mapping = GetNGOffsetMapping();
  const NGOffsetMapping* first_letter_mapping =
      first_letter->GetNGOffsetMapping();
  if (first_letter_mapping && remaining_text_mapping &&
      first_letter_mapping != remaining_text_mapping)
    return first_letter_mapping->GetText() + LayoutText::PlainText();
  return LayoutText::PlainText();
}

}  // namespace blink
