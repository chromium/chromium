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
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Pool>, pool, ());
    return *pool;
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
    DCHECK(U_SUCCESS(error_code) && iterator)
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

CharacterBreakIterator::CharacterBreakIterator(const StringView& string) {
  if (string.empty()) {
    is_8bit_ = true;
    return;
  }

  is_8bit_ = string.Is8Bit();

  if (is_8bit_) {
    base::span<const LChar> chars = string.Span8();
    charaters8_ = chars.data();
    offset_ = 0;
    // static_cast<> is safe because `chars` came from a StringView.
    length_ = static_cast<unsigned>(chars.size());
    return;
  }

  CreateIteratorForBuffer(string.Span16());
}

CharacterBreakIterator::CharacterBreakIterator(base::span<const UChar> buffer) {
  CreateIteratorForBuffer(buffer);
}

void CharacterBreakIterator::CreateIteratorForBuffer(
    base::span<const UChar> buffer) {
  iterator_ = Pool::Get().TakeOrCreate();
  SetText16(iterator_.get(), buffer);
}

int CharacterBreakIterator::Next() {
  if (!is_8bit_) {
    return iterator_->next();
  }

  if (offset_ >= length_) {
    return kTextBreakDone;
  }

  offset_ += ClusterLengthStartingAt(offset_);
  return offset_;
}

int CharacterBreakIterator::Current() {
  if (!is_8bit_) {
    return iterator_->current();
  }
  return offset_;
}

bool CharacterBreakIterator::IsBreak(int offset) const {
  if (!is_8bit_) {
    return iterator_->isBoundary(offset);
  }
  return !IsLfAfterCr(offset);
}

int CharacterBreakIterator::Preceding(int offset) const {
  if (!is_8bit_) {
    return iterator_->preceding(offset);
  }
  if (offset <= 0) {
    return kTextBreakDone;
  }
  if (IsLfAfterCr(offset)) {
    return offset - 2;
  }
  return offset - 1;
}

int CharacterBreakIterator::Following(int offset) const {
  if (!is_8bit_) {
    return iterator_->following(offset);
  }
  if (static_cast<unsigned>(offset) >= length_) {
    return kTextBreakDone;
  }
  return offset + ClusterLengthStartingAt(offset);
}

unsigned NumGraphemeClusters(const StringView& string) {
  unsigned string_length = string.length();

  if (!string_length) {
    return 0;
  }

  // The only Latin-1 Extended Grapheme Cluster is CR LF
  if (string.Is8Bit() && !string.contains('\r')) {
    return string_length;
  }

  CharacterBreakIterator it(string);
  if (!it) {
    return string_length;
  }

  unsigned num = 0;
  while (it.Next() != kTextBreakDone) {
    ++num;
  }
  return num;
}

void GraphemesClusterList(const StringView& text,
                          base::span<unsigned> graphemes) {
  const unsigned length = text.length();
  DCHECK_EQ(length, graphemes.size());
  if (!length) {
    return;
  }

  CharacterBreakIterator it(text);
  int cursor_pos = it.Next();
  unsigned count = 0;
  unsigned pos = 0;
  while (cursor_pos >= 0) {
    for (; pos < static_cast<unsigned>(cursor_pos) && pos < length; ++pos) {
      graphemes[pos] = count;
    }
    cursor_pos = it.Next();
    count++;
  }
}

unsigned LengthOfGraphemeCluster(const StringView& string, unsigned offset) {
  unsigned string_length = string.length();
  CHECK_LE(offset, string_length);

  if (string_length - offset <= 1) {
    return string_length - offset;
  }

  // The only Latin-1 Extended Grapheme Cluster is CRLF.
  if (string.Is8Bit()) {
    return (1 + UNSAFE_TODO(
                    (string[offset] == '\r' && string[offset + 1] == '\n')));
  }

  CharacterBreakIterator it(string);
  if (!it) {
    return string_length - offset;
  }

  if (it.Following(offset) == kTextBreakDone) {
    return string_length - offset;
  }
  return it.Current() - offset;
}

}  // namespace blink
