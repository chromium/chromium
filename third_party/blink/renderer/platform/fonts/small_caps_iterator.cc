// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"

#include <unicode/utypes.h>
#include <memory>

namespace blink {

SmallCapsIterator::SmallCapsIterator(const UChar* buffer, unsigned buffer_size)
    : utf16_iterator_(buffer, buffer_size),
      buffer_size_(buffer_size),
      next_u_char32_(0),
      at_end_(buffer_size == 0),
      current_small_caps_behavior_(kSmallCapsInvalid) {}

bool SmallCapsIterator::Consume(unsigned* caps_limit,
                                SmallCapsBehavior* small_caps_behavior) {
  if (at_end_)
    return false;

  while (utf16_iterator_.Consume(next_u_char32_)) {
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
      *caps_limit = utf16_iterator_.Offset();
      *small_caps_behavior = previous_small_caps_behavior_;
      return true;
    }
    utf16_iterator_.Advance();
  }
  *caps_limit = buffer_size_;
  *small_caps_behavior = current_small_caps_behavior_;
  at_end_ = true;
  return true;
}

}  // namespace blink
