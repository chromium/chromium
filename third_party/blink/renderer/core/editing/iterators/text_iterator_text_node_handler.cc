// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_iterator_text_node_handler.h"

#include <algorithm>
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_text_state.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

// A magic value for infinity, used to indicate that text emission should
// proceed till the end of the text node. Can be removed when we can handle text
// length differences due to text-transform correctly.
const unsigned kMaxOffset = std::numeric_limits<unsigned>::max();

bool ShouldSkipInvisibleTextAt(const Text& text,
                               unsigned offset,
                               bool ignores_visibility) {
  const LayoutObject* layout_object = AssociatedLayoutObjectOf(text, offset);
  if (!layout_object)
    return true;
  if (layout_object->Style()->Display() == EDisplay::kNone)
    return true;
  if (ignores_visibility)
    return false;
  return layout_object->Style()->Visibility() != EVisibility::kVisible;
}

struct StringAndOffsetRange {
  String string;
  unsigned start;
  unsigned end;
};

StringAndOffsetRange ComputeTextAndOffsetsForEmission(
    const NGOffsetMapping& mapping,
    const NGOffsetMappingUnit& unit,
    const TextIteratorBehavior& behavior) {
  // TODO(xiaochengh): Handle EmitsOriginalText.
  if (behavior.EmitsSpaceForNbsp()) {
    String string = mapping.GetText().Substring(
        unit.TextContentStart(),
        unit.TextContentEnd() - unit.TextContentStart());
    string.Replace(kNoBreakSpaceCharacter, kSpaceCharacter);
    return {string, 0, string.length()};
  }
  return {mapping.GetText(), unit.TextContentStart(), unit.TextContentEnd()};
}

}  // namespace

TextIteratorTextNodeHandler::TextIteratorTextNodeHandler(
    const TextIteratorBehavior& behavior,
    TextIteratorTextState* text_state)
    : behavior_(behavior), text_state_(*text_state) {}

bool TextIteratorTextNodeHandler::HandleRemainingTextRuns() {
  if (text_node_)
    HandleTextNodeWithLayoutNG();
  return text_state_.PositionNode();
}

void TextIteratorTextNodeHandler::HandleTextNodeWithLayoutNG() {
  DCHECK_LE(offset_, end_offset_);
  DCHECK_LE(end_offset_, text_node_->data().length());
  DCHECK_LE(mapping_units_index_, mapping_units_.size());

  while (offset_ < end_offset_ && !text_state_.PositionNode()) {
    const EphemeralRange range_to_emit(Position(text_node_, offset_),
                                       Position(text_node_, end_offset_));

    // We may go through multiple mappings, which happens when there is
    // ::first-letter and blockifying style.
    auto* mapping = NGOffsetMapping::ForceGetFor(range_to_emit.StartPosition());
    if (!mapping) {
      offset_ = end_offset_;
      return;
    }

    if (mapping_units_index_ >= mapping_units_.size()) {
      // mapping_units_ got in HandleTextNodeInRange() ran out. It was for
      // :first-letter. We call GetMappingUnitsForDOMRange() again for the
      // remaining part of |text_node_|.
      mapping_units_ = mapping->GetMappingUnitsForDOMRange(range_to_emit);
      mapping_units_index_ = 0;
    }

    const unsigned initial_offset = offset_;
    for (; mapping_units_index_ < mapping_units_.size();
         ++mapping_units_index_) {
      const auto& unit = mapping_units_[mapping_units_index_];
      if (unit.TextContentEnd() == unit.TextContentStart() ||
          ShouldSkipInvisibleTextAt(*text_node_, unit.DOMStart(),
                                    IgnoresStyleVisibility())) {
        offset_ = unit.DOMEnd();
        continue;
      }

      auto string_and_offsets =
          ComputeTextAndOffsetsForEmission(*mapping, unit, behavior_);
      const String& string = string_and_offsets.string;
      const unsigned text_content_start = string_and_offsets.start;
      const unsigned text_content_end = string_and_offsets.end;
      text_state_.EmitText(*text_node_, unit.DOMStart(), unit.DOMEnd(), string,
                           text_content_start, text_content_end);
      offset_ = unit.DOMEnd();
      ++mapping_units_index_;
      return;
    }

    // Bail if |offset_| isn't advanced; Otherwise we enter a dead loop.
    // However, this shouldn't happen and should be fixed once reached.
    if (offset_ == initial_offset) {
      NOTREACHED();
      offset_ = end_offset_;
      return;
    }
  }
}

bool TextIteratorTextNodeHandler::ShouldHandleFirstLetter(
    const LayoutText& layout_text) const {
  if (handled_first_letter_)
    return false;
  if (!layout_text.IsTextFragment())
    return false;
  const auto& text_fragment = To<LayoutTextFragment>(layout_text);
  return offset_ < text_fragment.TextStartOffset();
}

void TextIteratorTextNodeHandler::HandleTextNodeInRange(const Text* node,
                                                        unsigned start_offset,
                                                        unsigned end_offset) {
  DCHECK(node);

  // TODO(editing-dev): Stop passing in |start_offset == end_offset|.
  DCHECK_LE(start_offset, end_offset);

  text_node_ = node;
  offset_ = start_offset;
  end_offset_ = end_offset;
  handled_first_letter_ = false;
  first_letter_text_ = nullptr;
  mapping_units_.clear();

  const NGOffsetMapping* const mapping =
      NGOffsetMapping::ForceGetFor(Position(node, offset_));
  if (UNLIKELY(!mapping)) {
    NOTREACHED() << "We have LayoutText outside LayoutBlockFlow " << text_node_;
    return;
  }

  // Restore end offset from magic value.
  if (end_offset_ == kMaxOffset)
    end_offset_ = node->data().length();
  mapping_units_ = mapping->GetMappingUnitsForDOMRange(
      EphemeralRange(Position(node, offset_), Position(node, end_offset_)));
  mapping_units_index_ = 0;
  HandleTextNodeWithLayoutNG();
}

void TextIteratorTextNodeHandler::HandleTextNodeStartFrom(
    const Text* node,
    unsigned start_offset) {
  HandleTextNodeInRange(node, start_offset, kMaxOffset);
}

void TextIteratorTextNodeHandler::HandleTextNodeEndAt(const Text* node,
                                                      unsigned end_offset) {
  HandleTextNodeInRange(node, 0, end_offset);
}

void TextIteratorTextNodeHandler::HandleTextNodeWhole(const Text* node) {
  HandleTextNodeStartFrom(node, 0);
}

void TextIteratorTextNodeHandler::HandleTextNodeFirstLetter(
    LayoutTextFragment* layout_object) {
  handled_first_letter_ = true;

  if (!layout_object->IsRemainingTextLayoutObject())
    return;

  FirstLetterPseudoElement* first_letter_element =
      layout_object->GetFirstLetterPseudoElement();
  if (!first_letter_element)
    return;

  LayoutObject* pseudo_layout_object = first_letter_element->GetLayoutObject();
  if (pseudo_layout_object->Style()->Visibility() != EVisibility::kVisible &&
      !IgnoresStyleVisibility())
    return;

  LayoutObject* first_letter = pseudo_layout_object->SlowFirstChild();

  sorted_text_boxes_.clear();
  CHECK(first_letter && first_letter->IsText());
  first_letter_text_ = To<LayoutText>(first_letter);
}

void TextIteratorTextNodeHandler::EmitChar16Before(UChar code_unit,
                                                   unsigned offset) {
  text_state_.EmitChar16Before(code_unit, *text_node_, offset);
}

void TextIteratorTextNodeHandler::EmitReplacmentCodeUnit(UChar code_unit,
                                                         unsigned offset) {
  text_state_.EmitReplacmentCodeUnit(code_unit, *text_node_, offset);
}

void TextIteratorTextNodeHandler::EmitText(const LayoutText* layout_object,
                                           unsigned text_start_offset,
                                           unsigned text_end_offset) {
  String string = behavior_.EmitsOriginalText() ? layout_object->OriginalText()
                                                : layout_object->GetText();
  if (behavior_.EmitsSpaceForNbsp())
    string.Replace(kNoBreakSpaceCharacter, kSpaceCharacter);
  text_state_.EmitText(*text_node_,
                       text_start_offset + layout_object->TextStartOffset(),
                       text_end_offset + layout_object->TextStartOffset(),
                       string, text_start_offset, text_end_offset);
}

}  // namespace blink
