/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007, 2011, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_BREAK_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_BREAK_ITERATOR_H_

#include <unicode/brkiter.h>

#include <optional>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

using TextBreakIterator = icu::BreakIterator;

const int kTextBreakDone = -1;

// Iterates over "extended grapheme clusters", as defined in UAX #29.
// Note that platform implementations may be less sophisticated - e.g. ICU prior
// to version 4.0 only supports "legacy grapheme clusters".  Use this for
// general text processing, e.g. string truncation.

class PLATFORM_EXPORT CharacterBreakIterator final {
  STACK_ALLOCATED();

 public:
  explicit CharacterBreakIterator(const StringView&);
  explicit CharacterBreakIterator(base::span<const UChar>);
  CharacterBreakIterator(const CharacterBreakIterator&) = delete;
  CharacterBreakIterator& operator=(const CharacterBreakIterator&) = delete;

  // `Next()` and `Current()` iterate from the start. The other functions take
  // an offset, and return the same values as `icu::BreakIterator`, but don't
  // change `Current()`.
  int Next();
  int Current();

  bool IsBreak(int offset) const;
  int Preceding(int offset) const;
  int Following(int offset) const;

 private:
  struct PLATFORM_EXPORT ReturnToPool {
    void operator()(void* ptr) const;
  };
  using PooledIterator = std::unique_ptr<icu::BreakIterator, ReturnToPool>;
  class Pool;
  FRIEND_TEST_ALL_PREFIXES(TextBreakIteratorTest, PooledCharacterBreakIterator);

  // Returns whether there is a break before `offset`, which must be in
  // [0, length_), if the code units around it are Latin-1.
  std::optional<bool> IsLatin1Break(int offset) const;
  // Returns `Following(offset)`, for `offset` in [0, length_), if
  // `IsLatin1Break()` decides it.
  std::optional<int> Latin1Following(int offset) const;

  // Only 16-bit text can need the ICU iterator, which is created on demand.
  // The caller may move it, so this clears `icu_at_offset_`.
  icu::BreakIterator& GetIterator() const;

  bool is_8bit_ = true;
  // Whether `iterator_` is at `offset_`, so that `Next()` can use its `next()`.
  mutable bool icu_at_offset_ = false;
  base::span<const LChar> characters8_;
  base::span<const UChar> characters16_;
  int length_ = 0;
  int offset_ = 0;
  mutable PooledIterator iterator_;
};

// Counts the number of grapheme clusters. A surrogate pair or a sequence
// of a non-combining character and following combining characters is
// counted as 1 grapheme cluster.
PLATFORM_EXPORT wtf_size_t NumGraphemeClusters(const StringView&);

// Returns the number of code units that the next grapheme cluster is made of.
PLATFORM_EXPORT wtf_size_t LengthOfGraphemeCluster(const StringView&,
                                                   wtf_size_t = 0);

// Returns a list of graphemes cluster at each character using character break
// rules. The `graphemes` and `text` must have the same size.
PLATFORM_EXPORT void GraphemesClusterList(const StringView& text,
                                          base::span<wtf_size_t> graphemes);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_BREAK_ITERATOR_H_
