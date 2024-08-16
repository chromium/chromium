/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"

#include <unicode/utf16.h>

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

UnicodeRangeSet::UnicodeRangeSet(HeapVector<UnicodeRange>&& ranges)
    : ranges_(std::move(ranges)) {
  if (ranges_.empty())
    return;

  std::sort(ranges_.begin(), ranges_.end());

  // Unify overlapping ranges.
  UChar32 from = ranges_[0].From();
  UChar32 to = ranges_[0].To();
  wtf_size_t target_index = 0;
  for (wtf_size_t i = 1; i < ranges_.size(); i++) {
    if (to + 1 >= ranges_[i].From()) {
      to = std::max(to, ranges_[i].To());
    } else {
      ranges_[target_index++] = UnicodeRange(from, to);
      from = ranges_[i].From();
      to = ranges_[i].To();
    }
  }
  ranges_[target_index++] = UnicodeRange(from, to);
  ranges_.Shrink(target_index);
}

bool UnicodeRangeSet::Contains(UChar32 c) const {
  if (IsEntireRange())
    return true;
  HeapVector<UnicodeRange>::const_iterator it =
      std::lower_bound(ranges_.begin(), ranges_.end(), c);
  return it != ranges_.end() && it->Contains(c);
}

bool UnicodeRangeSet::IntersectsWith(const String& text) const {
  if (text.empty())
    return false;
  if (IsEntireRange())
    return true;
  if (text.Is8Bit() && ranges_[0].From() >= 0x100)
    return false;

  unsigned index = 0;
  while (index < text.length()) {
    UChar32 c = text.CharacterStartingAt(index);
    index += U16_LENGTH(c);
    if (Contains(c))
      return true;
  }
  return false;
}

bool UnicodeRangeSet::operator==(const UnicodeRangeSet& other) const {
  if (ranges_.size() == 0 && other.size() == 0)
    return true;
  if (ranges_.size() != other.size()) {
    return false;
  }
  bool equal = true;
  for (wtf_size_t i = 0; i < ranges_.size(); ++i) {
    equal = equal && ranges_[i] == other.ranges_[i];
  }
  return equal;
}
}
