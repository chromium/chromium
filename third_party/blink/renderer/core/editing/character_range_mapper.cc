// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/character_range_mapper.h"

namespace blink {

template <typename Strategy>
CharacterRange CharacterRangeMapperAlgorithm<Strategy>::CreateCharacterRange(
    const EphemeralRangeTemplate<Strategy>& scope,
    const EphemeralRangeTemplate<Strategy>& range,
    const TextIteratorBehavior& behavior) {
  DCHECK_GE(range.StartPosition(), scope.StartPosition());
  DCHECK_LE(range.EndPosition(), scope.EndPosition());

  int offset = 0;
  TextIteratorAlgorithm<Strategy> offset_iterator(
      scope.StartPosition(), range.StartPosition(), behavior);
  for (; !offset_iterator.AtEnd(); offset_iterator.Advance()) {
    offset += offset_iterator.length();
  }

  int length = 0;
  TextIteratorAlgorithm<Strategy> length_iterator(
      range.StartPosition(), range.EndPosition(), behavior);
  for (; !length_iterator.AtEnd(); length_iterator.Advance()) {
    length += length_iterator.length();
  }

  return {offset, length};
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
CharacterRangeMapperAlgorithm<Strategy>::ResolveCharacterRange(
    const EphemeralRangeTemplate<Strategy>& scope,
    const CharacterRange& range,
    const TextIteratorBehavior& behavior) {
  DCHECK_GE(range.offset, 0);
  DCHECK_GE(range.length, 0);

  const int start_offset = range.offset;
  const int end_offset = range.offset + range.length;
  const bool is_range_empty = !range.length;
  int length = 0;
  PositionTemplate<Strategy> start_position;
  PositionTemplate<Strategy> end_position;
  TextIteratorAlgorithm<Strategy> it(scope, behavior);
  for (; !it.AtEnd(); it.Advance()) {
    int current_length = it.length();

    // Use downstream affinity for computing the start position, unless the
    // selection range is empty.
    if (!is_range_empty && length <= start_offset &&
        start_offset < length + current_length) {
      const TextIteratorTextState& text_state = it.GetTextState();
      int offset_in_container = start_offset - length;
      if (text_state.PositionContainerNode()) {
        start_position = PositionTemplate<Strategy>(
            it.CurrentContainer(),
            text_state.PositionStartOffset() + offset_in_container);
      } else {
        start_position = PositionTemplate<Strategy>(it.CurrentContainer(),
                                                    offset_in_container);
      }
    }

    // Use upstream affinity for computing the end position, unless the range is
    // empty, where we may resolve to the first position within the scope.
    if (((is_range_empty && !length) || length < end_offset) &&
        end_offset <= length + current_length) {
      const TextIteratorTextState& text_state = it.GetTextState();
      int offset_in_container = end_offset - length;
      if (text_state.PositionContainerNode()) {
        end_position = PositionTemplate<Strategy>(
            it.CurrentContainer(),
            text_state.PositionStartOffset() + offset_in_container);
      } else {
        end_position = PositionTemplate<Strategy>(it.CurrentContainer(),
                                                  offset_in_container);
      }

      if (is_range_empty) {
        // The length of the range is zero therefore the start position is the
        // same as the end position.
        start_position = end_position;
      }

      // Found the end position. We don't need to iterate further.
      break;
    }

    length += current_length;
  }

  if (!start_position || !end_position) {
    DCHECK(false) << "Failed to resolve CharacterRange.";
    return EphemeralRangeTemplate<Strategy>();
  }

  return EphemeralRangeTemplate<Strategy>(start_position, end_position);
}

template class CORE_TEMPLATE_EXPORT
    CharacterRangeMapperAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    CharacterRangeMapperAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
