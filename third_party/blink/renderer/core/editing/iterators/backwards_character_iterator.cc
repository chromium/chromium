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

#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"

namespace blink {

template <typename Strategy>
BackwardsCharacterIteratorAlgorithm<Strategy>::
    BackwardsCharacterIteratorAlgorithm(
        const EphemeralRangeTemplate<Strategy>& range,
        const TextIteratorBehavior& behavior)
    : offset_(0),
      run_offset_(0),
      at_break_(true),
      text_iterator_(range, behavior) {
  while (!AtEnd() && !text_iterator_.length())
    text_iterator_.Advance();
}

template <typename Strategy>
PositionTemplate<Strategy>
BackwardsCharacterIteratorAlgorithm<Strategy>::EndPosition() const {
  if (!text_iterator_.AtEnd()) {
    if (text_iterator_.length() > 1) {
      const Node* n = text_iterator_.StartContainer();
      return PositionTemplate<Strategy>::EditingPositionOf(
          n, text_iterator_.EndOffset() - run_offset_);
    }
    DCHECK(!run_offset_);
  }
  return text_iterator_.EndPosition();
}

template <typename Strategy>
void BackwardsCharacterIteratorAlgorithm<Strategy>::Advance(int count) {
  if (count <= 0) {
    DCHECK(!count);
    return;
  }

  at_break_ = false;

  int remaining = text_iterator_.length() - run_offset_;
  if (count < remaining) {
    run_offset_ += count;
    offset_ += count;
    return;
  }

  count -= remaining;
  offset_ += remaining;

  for (text_iterator_.Advance(); !AtEnd(); text_iterator_.Advance()) {
    int run_length = text_iterator_.length();
    if (!run_length) {
      at_break_ = true;
    } else {
      if (count < run_length) {
        run_offset_ = count;
        offset_ += count;
        return;
      }

      count -= run_length;
      offset_ += run_length;
    }
  }

  at_break_ = true;
  run_offset_ = 0;
}

template class CORE_TEMPLATE_EXPORT
    BackwardsCharacterIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    BackwardsCharacterIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
