/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/platform/text/character_break_iterator.h"

#include <unicode/brkiter.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator_internal_icu.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

//
// A simple pool of `icu::BreakIterator` without any keys, as
// `CharacterBreakIterator` is locale-independent.
//
class CharacterBreakIterator::Pool {
 public:
  static Pool& Get() {
    static base::NoDestructor<ThreadSpecific<Pool>> pool;
    return **pool;
  }

  PooledIterator TakeOrCreate() {
    if (!pool_.empty()) {
      PooledIterator iterator(pool_.back().release());
      pool_.pop_back();
      return iterator;
    }

    IcuError error_code;
    PooledIterator iterator(icu::BreakIterator::createCharacterInstance(
        CurrentTextBreakIcuLocale(), error_code));
    CHECK(U_SUCCESS(error_code) && iterator)
        << "ICU could not open a break iterator: " << u_errorName(error_code)
        << " (" << error_code << ")";
    return iterator;
  }

  void Put(icu::BreakIterator* iterator) { pool_.push_back(iterator); }

 private:
  static constexpr size_t kCapacity = 4;
  Vector<std::unique_ptr<icu::BreakIterator>, kCapacity> pool_;
};

void CharacterBreakIterator::ReturnToPool::operator()(void* ptr) const {
  icu::BreakIterator* iterator = static_cast<icu::BreakIterator*>(ptr);
  DCHECK(iterator);
  Pool::Get().Put(iterator);
}

CharacterBreakIterator::CharacterBreakIterator(const StringView& string)
    : length_(base::checked_cast<int>(string.length())) {
  if (string.Is8Bit()) {
    characters8_ = string.Span8();
  } else {
    is_8bit_ = false;
    characters16_ = string.Span16();
  }
}

CharacterBreakIterator::CharacterBreakIterator(base::span<const UChar> buffer)
    : is_8bit_(false),
      characters16_(buffer),
      length_(base::checked_cast<int>(buffer.size())) {}

icu::BreakIterator& CharacterBreakIterator::GetIterator() const {
  DCHECK(!is_8bit_);
  if (!iterator_) {
    iterator_ = Pool::Get().TakeOrCreate();
    SetText16(iterator_.get(), characters16_);
  }
  icu_at_offset_ = false;
  return *iterator_;
}

std::optional<bool> CharacterBreakIterator::IsLatin1Break(int offset) const {
  // There is a break between two Latin-1 characters, except CR LF.
  if (is_8bit_) {
    return !offset || characters8_[offset - 1] != '\r' ||
           characters8_[offset] != '\n';
  }
  if (!RuntimeEnabledFeatures::CharacterBreakIteratorFastPathEnabled()) {
    return std::nullopt;
  }
  const UChar before = offset ? characters16_[offset - 1] : 0;
  const UChar after = characters16_[offset];
  if (before > 0xFF || after > 0xFF) {
    return std::nullopt;
  }
  return before != '\r' || after != '\n';
}

std::optional<int> CharacterBreakIterator::Latin1Following(int offset) const {
  const int next = offset + 1;
  if (next == length_) {
    return next;
  }
  const std::optional<bool> is_break = IsLatin1Break(next);
  if (!is_break) {
    return std::nullopt;
  }
  // CR LF is the only cluster of Latin-1 characters.
  return *is_break ? next : next + 1;
}

int CharacterBreakIterator::Next() {
  if (offset_ >= length_) {
    return kTextBreakDone;
  }
  // Once the text needs ICU, keep using its `next()`, so that such text isn't
  // slower than with ICU alone.
  const bool icu_at_offset = icu_at_offset_;
  if (!icu_at_offset) {
    if (const std::optional<int> next = Latin1Following(offset_)) {
      offset_ = *next;
      return offset_;
    }
  }
  icu::BreakIterator& iterator = GetIterator();
  DCHECK(!icu_at_offset || iterator.current() == offset_);
  offset_ = icu_at_offset ? iterator.next() : iterator.following(offset_);
  icu_at_offset_ = true;
  return offset_;
}

int CharacterBreakIterator::Current() {
  return offset_;
}

bool CharacterBreakIterator::IsBreak(int offset) const {
  if (offset <= 0 || offset >= length_) {
    return offset == 0 || offset == length_;
  }
  const std::optional<bool> is_break = IsLatin1Break(offset);
  return is_break ? *is_break : GetIterator().isBoundary(offset);
}

int CharacterBreakIterator::Preceding(int offset) const {
  if (offset <= 0 || offset > length_) {
    return offset <= 0 ? kTextBreakDone : length_;
  }
  const int previous = offset - 1;
  const std::optional<bool> is_break = IsLatin1Break(previous);
  if (!is_break) {
    return GetIterator().preceding(offset);
  }
  // CR LF is the only cluster of Latin-1 characters.
  return *is_break ? previous : previous - 1;
}

int CharacterBreakIterator::Following(int offset) const {
  if (offset < 0 || offset >= length_) {
    return offset < 0 ? 0 : kTextBreakDone;
  }
  const std::optional<int> next = Latin1Following(offset);
  return next ? *next : GetIterator().following(offset);
}

wtf_size_t NumGraphemeClusters(const StringView& string) {
  wtf_size_t string_length = string.length();

  if (!string_length) {
    return 0;
  }

  // The only Latin-1 Extended Grapheme Cluster is CR LF
  if (string.Is8Bit() && !string.contains('\r')) {
    return string_length;
  }

  CharacterBreakIterator it(string);
  wtf_size_t num = 0;
  while (it.Next() != kTextBreakDone) {
    ++num;
  }
  return num;
}

void GraphemesClusterList(const StringView& text,
                          base::span<wtf_size_t> graphemes) {
  const wtf_size_t length = text.length();
  DCHECK_EQ(length, graphemes.size());
  if (!length) {
    return;
  }

  CharacterBreakIterator it(text);
  int cursor_pos = it.Next();
  wtf_size_t count = 0;
  wtf_size_t pos = 0;
  while (cursor_pos >= 0) {
    for (; pos < static_cast<wtf_size_t>(cursor_pos) && pos < length; ++pos) {
      graphemes[pos] = count;
    }
    cursor_pos = it.Next();
    count++;
  }
}

wtf_size_t LengthOfGraphemeCluster(const StringView& string,
                                   wtf_size_t offset) {
  wtf_size_t string_length = string.length();
  CHECK_LE(offset, string_length);

  if (string_length - offset <= 1) {
    return string_length - offset;
  }

  // The only Latin-1 Extended Grapheme Cluster is CRLF.
  if (string.Is8Bit()) {
    return (1 + UNSAFE_TODO(
                    (string[offset] == '\r' && string[offset + 1] == '\n')));
  }

  // This isn't `kTextBreakDone` because `offset + 1 < string_length`.
  return CharacterBreakIterator(string).Following(offset) - offset;
}

}  // namespace blink
