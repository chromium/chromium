/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_entity_search.h"

#include <algorithm>
#include <functional>

namespace blink {

HTMLEntitySearch::HTMLEntitySearch() : range_(HTMLEntityTable::AllEntries()) {}

void HTMLEntitySearch::Advance(UChar next_character) {
  DCHECK(IsEntityPrefix());
  if (!current_length_) {
    range_ = HTMLEntityTable::EntriesStartingWith(next_character);
  } else {
    // Get the subrange where `next_character` matches at the end of the
    // current prefix (index == `current_length_`).
    auto projector =
        [this](const HTMLEntityTableEntry& entry) -> std::optional<UChar> {
      if (entry.length < current_length_ + 1) {
        return std::nullopt;
      }
      base::span<const LChar> entity_string =
          HTMLEntityTable::EntityString(entry);
      return entity_string[current_length_];
    };
    range_ = std::ranges::equal_range(range_, next_character, std::less{},
                                      projector);
  }
  if (range_.empty()) {
    return Fail();
  }
  ++current_length_;
  if (range_.front().length != current_length_) {
    return;
  }
  most_recent_match_ = &range_.front();
}

}  // namespace blink
