// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"

#include <unicode/utypes.h>

#include <memory>

#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

SmallCapsIterator::SmallCapsIterator(base::span<const UChar> buffer)
    : begin_(buffer), utf16_iterator_(begin_) {}

bool SmallCapsIterator::Consume(wtf_size_t* caps_limit,
                                SmallCapsBehavior* small_caps_behavior) {
  const CodePointIterator::Utf16 end = utf16_iterator_.EndForThis();
  if (utf16_iterator_ == end) {
    return false;
  }

  for (; utf16_iterator_ != end; ++utf16_iterator_) {
    next_u_char32_ = *utf16_iterator_;
    if (U_IS_SURROGATE(next_u_char32_)) [[unlikely]] {
      next_u_char32_ = uchar::kReplacementCharacter;
    }
    previous_small_caps_behavior_ = current_small_caps_behavior_;
    // Skipping over combining marks, as these combine with the small-caps
    // uppercased text as well and we do not need to split by their
    // individual case-ness.
    if (!u_getCombiningClass(next_u_char32_)) {
      current_small_caps_behavior_ =
          u_hasBinaryProperty(next_u_char32_, UCHAR_CHANGES_WHEN_UPPERCASED)
              ? kSmallCapsUppercaseNeeded
              : kSmallCapsSameCase;
    }

    if (previous_small_caps_behavior_ != current_small_caps_behavior_ &&
        previous_small_caps_behavior_ != kSmallCapsInvalid) {
      *caps_limit = utf16_iterator_.DistanceByCodeUnits(begin_);
      *small_caps_behavior = previous_small_caps_behavior_;
      return true;
    }
  }
  *caps_limit = end.DistanceByCodeUnits(begin_);
  *small_caps_behavior = current_small_caps_behavior_;
  return true;
}

}  // namespace blink
