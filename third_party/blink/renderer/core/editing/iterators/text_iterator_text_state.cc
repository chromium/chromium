/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "third_party/blink/renderer/core/editing/iterators/text_iterator_text_state.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool IsTextSecurityNode(const Node& node) {
  return node.GetLayoutObject() &&
         node.GetLayoutObject()->Style()->TextSecurity() !=
             ETextSecurity::kNone;
}

}  // anonymous namespace

TextIteratorTextState::TextIteratorTextState(
    const TextIteratorBehavior& behavior)
    : behavior_(behavior) {}

unsigned TextIteratorTextState::PositionStartOffset() const {
  DCHECK(position_container_node_);
  return position_start_offset_.value();
}

unsigned TextIteratorTextState::PositionEndOffset() const {
  DCHECK(position_container_node_);
  return position_end_offset_.value();
}

UChar TextIteratorTextState::CharacterAt(unsigned index) const {
  SECURITY_DCHECK(index < length());
  if (!(index < length()))
    return 0;

  if (single_character_buffer_) {
    DCHECK_EQ(index, 0u);
    DCHECK_EQ(length(), 1u);
    return single_character_buffer_;
  }

  return text_[text_start_offset_ + index];
}

String TextIteratorTextState::GetTextForTesting() const {
  if (single_character_buffer_)
    return String(&single_character_buffer_, 1);
  return text_.Substring(text_start_offset_, length());
}

void TextIteratorTextState::AppendTextToStringBuilder(
    StringBuilder& builder,
    unsigned position,
    unsigned max_length) const {
  SECURITY_DCHECK(position <= this->length());
  unsigned length_to_append = std::min(length() - position, max_length);
  if (!length_to_append)
    return;
  if (single_character_buffer_) {
    DCHECK_EQ(position, 0u);
    builder.Append(single_character_buffer_);
  } else {
    builder.Append(text_, text_start_offset_ + position, length_to_append);
  }
}

void TextIteratorTextState::UpdateForReplacedElement(const Node& node) {
  ResetPositionContainerNode(PositionNodeType::kAsNode, node);
  PopulateStringBuffer("", 0, 0);
}

void TextIteratorTextState::ResetPositionContainerNode(
    PositionNodeType node_type,
    const Node& node) {
  DCHECK_NE(node_type, PositionNodeType::kBeforeChildren);
  DCHECK_NE(node_type, PositionNodeType::kInText);
  DCHECK_NE(node_type, PositionNodeType::kNone);
  position_node_type_ = node_type;
  position_container_node_ = nullptr;
  position_node_ = node;
  position_start_offset_ = base::nullopt;
  position_end_offset_ = base::nullopt;
}

void TextIteratorTextState::UpdatePositionOffsets(
    const ContainerNode& container_node,
    unsigned node_index) const {
  DCHECK(!position_container_node_);
  DCHECK(!position_start_offset_.has_value());
  DCHECK(!position_end_offset_.has_value());
  switch (position_node_type_) {
    case PositionNodeType::kAfterNode:
      position_container_node_ = &container_node;
      position_start_offset_ = node_index + 1;
      position_end_offset_ = node_index + 1;
      return;
    case PositionNodeType::kAltText:
    case PositionNodeType::kAsNode:
      position_container_node_ = &container_node;
      position_start_offset_ = node_index;
      position_end_offset_ = node_index + 1;
      return;
    case PositionNodeType::kBeforeNode:
      position_container_node_ = &container_node;
      position_start_offset_ = node_index;
      position_end_offset_ = node_index;
      return;
    case PositionNodeType::kBeforeCharacter:
    case PositionNodeType::kBeforeChildren:
    case PositionNodeType::kInText:
    case PositionNodeType::kNone:
      NOTREACHED();
      return;
  }
  NOTREACHED() << static_cast<int>(position_node_type_);
}

void TextIteratorTextState::EmitAltText(const HTMLElement& element) {
  ResetPositionContainerNode(PositionNodeType::kAltText, element);
  const String text = element.AltText();
  PopulateStringBuffer(text, 0, text.length());
}

void TextIteratorTextState::EmitChar16AfterNode(UChar code_unit,
                                                const Node& node) {
  ResetPositionContainerNode(PositionNodeType::kAfterNode, node);
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::EmitChar16AsNode(UChar code_unit,
                                             const Node& node) {
  ResetPositionContainerNode(PositionNodeType::kAsNode, node);
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::EmitChar16BeforeChildren(
    UChar code_unit,
    const ContainerNode& container_node) {
  position_node_type_ = PositionNodeType::kBeforeChildren;
  position_container_node_ = &container_node;
  position_node_ = &container_node;
  position_start_offset_ = 0;
  position_end_offset_ = 0;
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::EmitChar16BeforeNode(UChar code_unit,
                                                 const Node& node) {
  ResetPositionContainerNode(PositionNodeType::kBeforeNode, node);
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::EmitChar16Before(UChar code_unit,
                                             const Text& text_node,
                                             unsigned offset) {
  // TODO(editing-dev): text-transform:uppercase can make text longer, e.g.
  // "U+00DF" to "SS". See "fast/css/case-transform.html"
  // DCHECK_LE(offset, text_node.length());
  position_node_type_ = PositionNodeType::kBeforeCharacter;
  position_container_node_ = &text_node;
  position_node_ = &text_node;
  position_start_offset_ = offset;
  position_end_offset_ = offset;
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::EmitReplacmentCodeUnit(UChar code_unit,
                                                   const Text& text_node,
                                                   unsigned offset) {
  SetTextNodePosition(text_node, offset, offset + 1);
  PopulateStringBufferFromChar16(code_unit);
}

void TextIteratorTextState::PopulateStringBufferFromChar16(UChar code_unit) {
  has_emitted_ = true;
  // remember information with which to construct the TextIterator::characters()
  // and length()
  single_character_buffer_ = code_unit;
  DCHECK(single_character_buffer_);
  text_length_ = 1;
  text_start_offset_ = 0;

  // remember some iteration state
  last_character_ = code_unit;
}

void TextIteratorTextState::EmitText(const Text& text_node,
                                     unsigned position_start_offset,
                                     unsigned position_end_offset,
                                     const String& string,
                                     unsigned text_start_offset,
                                     unsigned text_end_offset) {
  DCHECK_LE(position_start_offset, position_end_offset);
  const String text =
      behavior_.EmitsSmallXForTextSecurity() && IsTextSecurityNode(text_node)
          ? RepeatString("x", string.length())
          : string;

  DCHECK(!text.IsEmpty());
  DCHECK_LT(text_start_offset, text.length());
  DCHECK_LE(text_end_offset, text.length());
  DCHECK_LE(text_start_offset, text_end_offset);

  SetTextNodePosition(text_node, position_start_offset, position_end_offset);
  PopulateStringBuffer(text, text_start_offset, text_end_offset);
}

void TextIteratorTextState::PopulateStringBuffer(const String& text,
                                                 unsigned text_start_offset,
                                                 unsigned text_end_offset) {
  DCHECK_LE(text_start_offset, text_end_offset);
  DCHECK_LE(text_end_offset, text.length());
  text_ = text;
  single_character_buffer_ = 0;
  text_start_offset_ = text_start_offset;
  text_length_ = text_end_offset - text_start_offset;
  last_character_ = text_end_offset == 0 ? 0 : text_[text_end_offset - 1];

  has_emitted_ = true;
}

void TextIteratorTextState::SetTextNodePosition(const Text& text_node,
                                                unsigned position_start_offset,
                                                unsigned position_end_offset) {
  DCHECK_LT(position_start_offset, position_end_offset);
  // TODO(editing-dev): text-transform:uppercase can make text longer, e.g.
  // "U+00DF" to "SS". See "fast/css/case-transform.html"
  // DCHECK_LE(position_end_offset, text_node.length());
  position_node_type_ = PositionNodeType::kInText;
  position_container_node_ = &text_node;
  position_node_ = &text_node;
  position_start_offset_ = position_start_offset;
  position_end_offset_ = position_end_offset;
}

}  // namespace blink
