// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_iterator_text_node_handler.h"

#include <algorithm>
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_text_state.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

namespace {

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
  return layout_object->Style()->UsedVisibility() != EVisibility::kVisible;
}

String TextIgnoringCSSTextTransforms(const LayoutText& layout_text,
                                     const OffsetMappingUnit& unit) {
  // LayoutTextFragment represents text substring of the element that is split
  // because of first-letter css. In that case, OriginalText() returns only a
  // portion of the text. Use CompleteText() instead to get all text from the
  // associated DOM node.
  String text = layout_text.IsTextFragment()
                    ? To<LayoutTextFragment>(layout_text).CompleteText()
                    : layout_text.OriginalText();
  text = text.Substring(unit.DOMStart(), unit.DOMEnd() - unit.DOMStart());
  // Per the white space processing spec
  // https://drafts.csswg.org/css-text-3/#white-space-processing,
  // collapsed spaces should be ignored completely and this is assured since
  // |ComputeTextAndOffsetsForEmission| is not called for kCollapsed unit.
  // Preserved whitespaces can be represented as-is.
  // Non-preserved newline or tab characters should be converted into a space
  // to reflect what the user sees on the screen
  if (!layout_text.StyleRef().ShouldPreserveBreaks()) {
    text.Replace(kNewlineCharacter, kSpaceCharacter);
    text.Replace(kTabulationCharacter, kSpaceCharacter);
  }
  return text;
}

struct StringAndOffsetRange {
  String string;
  unsigned start;
  unsigned end;
};

StringAndOffsetRange ComputeTextAndOffsetsForEmission(
    const OffsetMapping& mapping,
    const OffsetMappingUnit& unit,
    const TextIteratorBehavior& behavior) {
  StringAndOffsetRange result{mapping.GetText(), unit.TextContentStart(),
                              unit.TextContentEnd()};

  // This is ensured because |unit.GetLayoutObject()| must be the
  // LayoutObject for TextIteratorTextNodeHandler's |text_node_|.
  DCHECK(IsA<LayoutText>(unit.GetLayoutObject()));
  const LayoutText& layout_text = To<LayoutText>(unit.GetLayoutObject());

  // |TextIgnoringCSSTextTransforms| gets |layout_text.OriginalText()|
  // which is not masked. This should not be allowed when
  // |-webkit-text-security| property is set.
  if (behavior.IgnoresCSSTextTransforms() && layout_text.HasTextTransform() &&
      !layout_text.IsSecure()) {
    result.string = TextIgnoringCSSTextTransforms(layout_text, unit);
    result.start = 0;
    result.end = result.string.length();
  }

  if (behavior.EmitsOriginalText()) {
    result.string = layout_text.OriginalText().Substring(
        unit.DOMStart(), unit.DOMEnd() - unit.DOMStart());
    result.start = 0;
    result.end = result.string.length();
  }

  if (behavior.EmitsSpaceForNbsp()) {
    result.string =
        result.string.Substring(result.start, result.end - result.start);
    result.string.Replace(kNoBreakSpaceCharacter, kSpaceCharacter);
    result.start = 0;
    result.end = result.string.length();
  }

  return result;
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
    auto* mapping = OffsetMapping::ForceGetFor(range_to_emit.StartPosition());
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
      DUMP_WILL_BE_NOTREACHED();
      offset_ = end_offset_;
      return;
    }
  }
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
  mapping_units_.clear();

  const OffsetMapping* const mapping =
      OffsetMapping::ForceGetFor(Position(node, offset_));
  if (!mapping) [[unlikely]] {
    DUMP_WILL_BE_NOTREACHED()
        << "We have LayoutText outside LayoutBlockFlow " << text_node_;
    return;
  }

  mapping_units_ = mapping->GetMappingUnitsForDOMRange(
      EphemeralRange(Position(node, offset_), Position(node, end_offset_)));
  mapping_units_index_ = 0;
  HandleTextNodeWithLayoutNG();
}

void TextIteratorTextNodeHandler::HandleTextNodeStartFrom(
    const Text* node,
    unsigned start_offset) {
  HandleTextNodeInRange(node, start_offset, node->data().length());
}

void TextIteratorTextNodeHandler::HandleTextNodeEndAt(const Text* node,
                                                      unsigned end_offset) {
  HandleTextNodeInRange(node, 0, end_offset);
}

void TextIteratorTextNodeHandler::HandleTextNodeWhole(const Text* node) {
  HandleTextNodeStartFrom(node, 0);
}

}  // namespace blink
