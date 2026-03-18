// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CHARACTER_RANGE_MAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CHARACTER_RANGE_MAPPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"

namespace blink {

struct CharacterRange {
  int offset;
  int length;

  bool operator==(const CharacterRange&) const = default;
};

// Converts between |EphemeralRange| and |CharacterRange| using a text iterator.
//
// A |CharacterRange| describes a subrange within a given "scope" range, using a
// character offset from the start of the scope and a character length.
//
// For a |TextControlElement|, the scope must be a range within the
// |InnerEditorElement| or a descendant of it.
//
// Examples (scope in brackets):
// - "[Hello, |world!|]" -> { offset: 7, length: 6 }
// - "[|Hello|, world!]" -> { offset: 0, length: 5 }
// - "Hello, [|world|!]" -> { offset: 0, length: 5 }
template <typename Strategy>
class CharacterRangeMapperAlgorithm {
 public:
  CharacterRangeMapperAlgorithm() = delete;

  // Converts an |EphemeralRange| into a |CharacterRange| relative to the
  // |scope| range.
  static CharacterRange CreateCharacterRange(
      const EphemeralRangeTemplate<Strategy>& scope,
      const EphemeralRangeTemplate<Strategy>& range,
      const TextIteratorBehavior& =
          TextIteratorBehavior::DefaultRangeLengthBehavior());

  // Converts a |CharacterRange| relative to the |scope| range into
  // an |EphemeralRange|.
  static EphemeralRangeTemplate<Strategy> ResolveCharacterRange(
      const EphemeralRangeTemplate<Strategy>& scope,
      const CharacterRange& range,
      const TextIteratorBehavior& =
          TextIteratorBehavior::DefaultRangeLengthBehavior());
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    CharacterRangeMapperAlgorithm<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    CharacterRangeMapperAlgorithm<EditingInFlatTreeStrategy>;

using CharacterRangeMapper = CharacterRangeMapperAlgorithm<EditingStrategy>;
using CharacterRangeMapperInFlatTree =
    CharacterRangeMapperAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CHARACTER_RANGE_MAPPER_H_
