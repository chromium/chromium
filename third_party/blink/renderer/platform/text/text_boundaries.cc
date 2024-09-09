/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Dominik Roettsches <dominik.roettsches@access-company.com>
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

#include "third_party/blink/renderer/platform/text/text_boundaries.h"

#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

int FindNextWordForward(base::span<const UChar> chars, int position) {
  TextBreakIterator* it = WordBreakIterator(chars);

  int len = base::checked_cast<int>(chars.size());
  position = it->following(position);
  while (position != kTextBreakDone) {
    // We stop searching when the character preceeding the break
    // is alphanumeric or underscore.
    if (position < len && (WTF::unicode::IsAlphanumeric(chars[position - 1]) ||
                           chars[position - 1] == kLowLineCharacter)) {
      return position;
    }

    position = it->following(position);
  }

  return len;
}

int FindNextWordBackward(base::span<const UChar> chars, int position) {
  TextBreakIterator* it = WordBreakIterator(chars);

  position = it->preceding(position);
  while (position != kTextBreakDone) {
    // We stop searching when the character following the break
    // is alphanumeric or underscore.
    if (position > 0 && (WTF::unicode::IsAlphanumeric(chars[position]) ||
                         chars[position] == kLowLineCharacter))
      return position;

    position = it->preceding(position);
  }

  return 0;
}

int FindWordStartBoundary(base::span<const UChar> chars, int position) {
  TextBreakIterator* it = WordBreakIterator(chars);
  it->following(position);
  return it->previous();
}

int FindWordEndBoundary(base::span<const UChar> chars, int position) {
  TextBreakIterator* it = WordBreakIterator(chars);
  int end = it->following(position);
  return end < 0 ? it->last() : end;
}

}  // namespace blink
