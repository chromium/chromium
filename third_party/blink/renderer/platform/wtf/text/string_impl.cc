/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

#include <algorithm>
#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/i18n/string_search.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode_string.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

using std::numeric_limits;

namespace blink {

namespace {

struct SameSizeAsStringImpl {
#if DCHECK_IS_ON()
  unsigned int ref_count_change_count;
#endif
  int fields[3];
};

ASSERT_SIZE(StringImpl, SameSizeAsStringImpl);

std::u16string ToU16String(base::span<const LChar> chars) {
  std::u16string s;
  s.reserve(chars.size());

  for (size_t i = 0u; i < chars.size(); ++i) {
    s.push_back(chars[i]);
  }
  return s;
}

std::u16string ToU16String(base::span<const UChar> chars) {
  return std::u16string(base::as_string_view(chars));
}

std::u16string ToU16String(const StringView& s) {
  return VisitCharacters(s, [](auto chars) { return ToU16String(chars); });
}

template <typename DestChar, typename SrcChar>
void CopyAndReplace(base::span<DestChar> dest,
                    base::span<const SrcChar> src,
                    DestChar old_char,
                    DestChar new_char) {
  for (size_t i = 0; i < src.size(); ++i) {
    DestChar ch = src[i];
    if (ch == old_char) {
      ch = new_char;
    }
    dest[i] = ch;
  }
}

// Compute the new size for a string with the original length of `length` after
// replacing `match_count` matches of `old_pattern_length` with
// `new_pattern_length`. Used by the various Replace() variants.
wtf_size_t ComputeSizeAfterReplacement(wtf_size_t length,
                                       wtf_size_t match_count,
                                       wtf_size_t old_pattern_length,
                                       wtf_size_t new_pattern_length) {
  const base::CheckedNumeric<wtf_size_t> checked_match_count(match_count);
  base::CheckedNumeric<wtf_size_t> checked_new_size(length);
  checked_new_size -= checked_match_count * old_pattern_length;
  checked_new_size += checked_match_count * new_pattern_length;
  return checked_new_size.ValueOrDie();
}

void CopyStringFragment(const StringView& fragment,
                        base::span<UChar> destination) {
  CHECK(!fragment.IsNull());
  auto destination_fragment = destination.first(fragment.length());
  if (fragment.Is8Bit()) {
    StringImpl::CopyChars(destination_fragment, fragment.Span8());
  } else {
    destination_fragment.copy_from(fragment.Span16());
  }
}

void CopyStringFragment(const StringView& fragment,
                        base::span<LChar> destination) {
  CHECK(!fragment.IsNull());
  destination.copy_prefix_from(fragment.Span8());
}

}  // namespace

void* StringImpl::operator new(size_t size) {
  DCHECK_EQ(size, sizeof(StringImpl));
  return Partitions::BufferMalloc(size, "blink::StringImpl");
}

void StringImpl::operator delete(void* ptr) {
  Partitions::BufferFree(ptr);
}

void StringImpl::operator delete(void* ptr, size_t size) {
  Partitions::BufferFreeWithSize(ptr, size);
}

inline StringImpl::~StringImpl() {
  DCHECK(!IsStatic());
}

void StringImpl::DestroyIfNeeded() const {
  if (hash_and_flags_.load(std::memory_order_acquire) & kIsAtomic) {
    // TODO: Remove const_cast
    if (AtomicStringTable::Instance().ReleaseAndRemoveIfNeeded(
            const_cast<StringImpl*>(this))) {
      // Use sized deallocation. We explicitly pass `GetAllocatedSize()` because
      // StringImpl instances are allocated with a dynamic size
      operator delete(const_cast<StringImpl*>(this), GetAllocatedSize());
    } else {
      // AtomicStringTable::Add() revived this before we started really
      // killing it.
    }
  } else {
    // This is not necessary but TSAN bots don't like the load in the
    // caller to have relaxed memory order. Adding this check here instead
    // of changing the load memory order to minimize perf impact.
    int ref_count = ref_count_.load(std::memory_order_acquire);
    DCHECK_EQ(ref_count, 1);
    operator delete(const_cast<StringImpl*>(this), GetAllocatedSize());
  }
}

unsigned StringImpl::ComputeASCIIFlags() const {
  AsciiStringAttributes ascii_attributes = VisitCharacters(
      *this, [](auto chars) { return CharacterAttributes(chars); });
  uint32_t new_flags = AsciiStringAttributesToFlags(ascii_attributes);
  const uint32_t previous_flags =
      hash_and_flags_.fetch_or(new_flags, std::memory_order_relaxed);
  static constexpr uint32_t mask =
      kAsciiPropertyCheckDone | kContainsOnlyAscii | kIsLowerAscii;
  DCHECK((previous_flags & mask) == 0 || (previous_flags & mask) == new_flags);
  return new_flags;
}

#if DCHECK_IS_ON()
std::string StringImpl::AsciiForDebugging() const {
  return String(IsolatedCopy()->Substring(0, 128)).Ascii();
}
#endif

scoped_refptr<StringImpl> StringImpl::CreateUninitialized(
    size_t length,
    base::span<LChar>& data) {
  if (!length) {
    data = {};
    return empty_;
  }
  const wtf_size_t narrowed_length = base::checked_cast<wtf_size_t>(length);

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = new (Partitions::BufferMalloc(
      AllocationSize<LChar>(narrowed_length), "blink::StringImpl"))
      StringImpl(narrowed_length, kForce8BitConstructor);

  data = string->CharacterBuffer<LChar>();
  return base::AdoptRef(string);
}

scoped_refptr<StringImpl> StringImpl::CreateUninitialized(
    size_t length,
    base::span<UChar>& data) {
  if (!length) {
    data = {};
    return empty_;
  }
  const wtf_size_t narrowed_length = base::checked_cast<wtf_size_t>(length);

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = new (Partitions::BufferMalloc(
      AllocationSize<UChar>(narrowed_length), "blink::StringImpl"))
      StringImpl(narrowed_length);

  data = string->CharacterBuffer<UChar>();
  return base::AdoptRef(string);
}

static StaticStringsTable& StaticStrings() {
  DEFINE_STATIC_LOCAL(StaticStringsTable, static_strings, ());
  return static_strings;
}

#if DCHECK_IS_ON()
static bool g_allow_creation_of_static_strings = true;
#endif

const StaticStringsTable& StringImpl::AllStaticStrings() {
  return StaticStrings();
}

void StringImpl::FreezeStaticStrings() {
  DCHECK(IsMainThread());

#if DCHECK_IS_ON()
  g_allow_creation_of_static_strings = false;
#endif
}

wtf_size_t StringImpl::highest_static_string_length_ = 0;

DEFINE_GLOBAL(, StringImpl, g_global_empty);
DEFINE_GLOBAL(, StringImpl, g_global_empty16_bit);
// Callers need the global empty strings to be non-const.
StringImpl* StringImpl::empty_ = const_cast<StringImpl*>(&g_global_empty);
StringImpl* StringImpl::empty16_bit_ =
    const_cast<StringImpl*>(&g_global_empty16_bit);
void StringImpl::InitStatics() {
  new ((void*)empty_) StringImpl(kConstructEmptyString);
  new ((void*)empty16_bit_) StringImpl(kConstructEmptyString16Bit);
  WTF_ANNOTATE_BENIGN_RACE(StringImpl::empty_,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::empty");
  WTF_ANNOTATE_BENIGN_RACE(StringImpl::empty16_bit_,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::empty16Bit");
}

StringImpl* StringImpl::CreateStatic(base::span<const char> string) {
#if DCHECK_IS_ON()
  DCHECK(g_allow_creation_of_static_strings);
#endif
  DCHECK(!string.empty());
  DCHECK(string.data());

  unsigned hash =
      StringHasher::ComputeHashAndMaskTop8Bits(string.data(), string.size());

  StaticStringsTable::const_iterator it = StaticStrings().find(hash);
  if (it != StaticStrings().end()) {
    DCHECK_EQ(base::as_string_view(it->value->Span8()),
              base::as_string_view(string));
    return it->value;
  }
  const wtf_size_t narrowed_length = static_cast<wtf_size_t>(string.size());

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  WTF_INTERNAL_LEAK_SANITIZER_DISABLED_SCOPE;
  StringImpl* impl = new (Partitions::BufferMalloc(
      AllocationSize<LChar>(narrowed_length), "blink::StringImpl"))
      StringImpl(narrowed_length, hash, kStaticString);

  impl->CharacterBuffer<LChar>().copy_from(base::as_bytes(string));
#if DCHECK_IS_ON()
  impl->AssertHashIsCorrect();
#endif

  DCHECK(IsMainThread());
  highest_static_string_length_ =
      std::max(highest_static_string_length_, narrowed_length);
  StaticStrings().insert(hash, impl);
  WTF_ANNOTATE_BENIGN_RACE(impl,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::createStatic");

  return impl;
}

void StringImpl::ReserveStaticStringsCapacityForSize(wtf_size_t size) {
#if DCHECK_IS_ON()
  DCHECK(g_allow_creation_of_static_strings);
#endif
  StaticStrings().ReserveCapacityForSize(size);
}

scoped_refptr<StringImpl> StringImpl::Create(
    base::span<const UChar> utf16_data) {
  if (utf16_data.empty()) {
    return empty_;
  }
  base::span<UChar> string_data;
  scoped_refptr<StringImpl> string =
      CreateUninitialized(utf16_data.size(), string_data);
  string_data.copy_from(utf16_data);
  return string;
}

scoped_refptr<StringImpl> StringImpl::Create(
    base::span<const LChar> latin1_data) {
  if (latin1_data.empty()) {
    return empty_;
  }
  base::span<LChar> string_data;
  scoped_refptr<StringImpl> string =
      CreateUninitialized(latin1_data.size(), string_data);
  string_data.copy_from(latin1_data);
  return string;
}

scoped_refptr<StringImpl> StringImpl::Create(
    base::span<const LChar> characters,
    AsciiStringAttributes ascii_attributes) {
  scoped_refptr<StringImpl> ret = Create(characters);
  if (!characters.empty()) {
    // If length is 0 then `ret` is empty_ and should not have its
    // attributes calculated or changed.
    uint32_t new_flags = AsciiStringAttributesToFlags(ascii_attributes);
    ret->hash_and_flags_.fetch_or(new_flags, std::memory_order_relaxed);
  }

  return ret;
}

scoped_refptr<StringImpl> StringImpl::Create8BitIfPossible(
    base::span<const UChar> characters) {
  if (!characters.data() || characters.empty()) {
    return empty_;
  }

  base::span<LChar> data;
  scoped_refptr<StringImpl> string =
      CreateUninitialized(characters.size(), data);

  for (size_t i = 0; i < characters.size(); ++i) {
    const UChar c = characters[i];
    if (c & 0xff00) {
      return Create(characters);
    }
    data[i] = static_cast<LChar>(c);
  }
  return string;
}

bool StringImpl::ContainsOnlyWhitespaceOrEmpty() {
  // FIXME: The definition of whitespace here includes a number of characters
  // that are not whitespace from the point of view of LayoutText; I wonder if
  // that's a problem in practice.
  return VisitCharacters(*this, [](const auto& str) {
    return std::ranges::all_of(str, [](auto ch) { return IsASCIISpace(ch); });
  });
}

scoped_refptr<StringImpl> StringImpl::Substring(wtf_size_t start,
                                                wtf_size_t length) const {
  if (start >= length_)
    return empty_;
  wtf_size_t max_length = length_ - start;
  if (length >= max_length) {
    // RefPtr has trouble dealing with const arguments. It should be updated
    // so this const_cast is not necessary.
    if (!start)
      return const_cast<StringImpl*>(this);
    length = max_length;
  }
  if (Is8Bit())
    return Create(Span8().subspan(start, length));

  return Create(Span16().subspan(start, length));
}

UChar32 StringImpl::CharacterStartingAt(wtf_size_t i) {
  if (Is8Bit()) {
    return Span8()[i];
  }
  const UChar32 c = CodePointAt(Span16(), i);
  return U_IS_SURROGATE(c) ? 0 : c;
}

size_t StringImpl::CopyTo(base::span<UChar> buffer, wtf_size_t start) const {
  size_t number_of_characters_to_copy =
      std::min<size_t>(length() - start, buffer.size());
  if (!number_of_characters_to_copy)
    return 0;
  buffer = buffer.first(number_of_characters_to_copy);
  VisitCharacters(StringView(*this, start, number_of_characters_to_copy),
                  [buffer](auto chars) { CopyChars(buffer, chars); });
  return number_of_characters_to_copy;
}

class StringImplAllocator {
 public:
  using ResultStringType = scoped_refptr<StringImpl>;

  template <typename CharType>
  scoped_refptr<StringImpl> Alloc(wtf_size_t length,
                                  base::span<CharType>& buffer) {
    return StringImpl::CreateUninitialized(length, buffer);
  }

  scoped_refptr<StringImpl> CoerceOriginal(const StringImpl& string) {
    return const_cast<StringImpl*>(&string);
  }
};

scoped_refptr<StringImpl> StringImpl::LowerASCII() {
  return ConvertAsciiCase(*this, LowerConverter(), StringImplAllocator());
}

scoped_refptr<StringImpl> StringImpl::UpperASCII() {
  return ConvertAsciiCase(*this, UpperConverter(), StringImplAllocator());
}

scoped_refptr<StringImpl> StringImpl::Fill(UChar character) {
  if (!(character & ~0x7F)) {
    base::span<LChar> data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
    std::ranges::fill(data, static_cast<LChar>(character));
    return new_impl;
  }
  base::span<UChar> data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
  std::ranges::fill(data, character);
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::FoldCase() {
  CHECK_LE(length_, static_cast<wtf_size_t>(numeric_limits<int32_t>::max()));

  if (Is8Bit()) {
    // Do a faster loop for the case where all the characters are ASCII.
    base::span<LChar> data8;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data8);
    LChar ored = 0;

    const base::span<const LChar> source8 = Span8();
    for (size_t i = 0; i < source8.size(); ++i) {
      const LChar c = source8[i];
      data8[i] = ToASCIILower(c);
      ored |= c;
    }

    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    for (size_t i = 0; i < source8.size(); ++i) {
      data8[i] = static_cast<LChar>(unicode::ToLower(source8[i]));
    }
    return new_impl;
  }

  // Do a faster loop for the case where all the characters are ASCII.
  base::span<UChar> data16;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data16);
  UChar ored = 0;

  const base::span<const UChar> source16 = Span16();
  for (size_t i = 0; i < source16.size(); ++i) {
    const UChar c = source16[i];
    data16[i] = ToASCIILower(c);
    ored |= c;
  }
  if (!(ored & ~0x7F))
    return new_impl;

  // Do a slower implementation for cases that include non-ASCII characters.
  bool error;
  const int32_t real_length = unicode::FoldCase(
      data16.data(), static_cast<int32_t>(data16.size()), source16.data(),
      static_cast<int32_t>(source16.size()), &error);
  if (!error && real_length == static_cast<int32_t>(data16.size())) {
    return new_impl;
  }
  new_impl = CreateUninitialized(real_length, data16);
  unicode::FoldCase(data16.data(), static_cast<int32_t>(data16.size()),
                    source16.data(), static_cast<int32_t>(source16.size()),
                    &error);
  if (error)
    return this;
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Truncate(wtf_size_t length) {
  if (length >= length_)
    return this;
  if (Is8Bit())
    return Create(Span8().first(length));
  return Create(Span16().first(length));
}

namespace {

using CharacterRange = std::pair<size_t, size_t>;

template <class UCharPredicate>
inline CharacterRange StrippedMatchedCharactersRange(const StringImpl& impl,
                                                     UCharPredicate predicate) {
  return VisitCharacters(impl, [predicate](auto characters) -> CharacterRange {
    if (characters.empty()) {
      return {0, 0};
    }

    size_t start = 0;
    size_t end = characters.size() - 1;

    // Skip white space from the start.
    while (start <= end && predicate(characters[start])) {
      ++start;
    }

    // String only contains matching characters.
    if (start > end) {
      return {0, 0};
    }

    // Skip white space from the end.
    while (end && predicate(characters[end])) {
      --end;
    }
    return {start, end + 1};
  });
}

}  // namespace

template <class UCharPredicate>
inline scoped_refptr<StringImpl> StringImpl::StripMatchedCharacters(
    UCharPredicate predicate) {
  const auto [start, end] = StrippedMatchedCharactersRange(*this, predicate);
  if (start == end) {
    return empty_;
  }
  if (start == 0 && end == length_) {
    return this;
  }
  if (Is8Bit())
    return Create(Span8().subspan(start, end - start));
  return Create(Span16().subspan(start, end - start));
}

class UCharPredicate final {
  STACK_ALLOCATED();

 public:
  inline UCharPredicate(CharacterMatchFunctionPtr function)
      : function_(function) {}

  inline bool operator()(UChar ch) const { return function_(ch); }

 private:
  const CharacterMatchFunctionPtr function_;
};

class SpaceOrNewlinePredicate final {
  STACK_ALLOCATED();

 public:
  inline bool operator()(UChar ch) const {
    return unicode::IsSpaceOrNewline(ch);
  }
};

wtf_size_t StringImpl::LengthWithStrippedWhiteSpace() const {
  const auto [start, end] =
      StrippedMatchedCharactersRange(*this, SpaceOrNewlinePredicate());
  return static_cast<wtf_size_t>(end - start);
}

scoped_refptr<StringImpl> StringImpl::StripWhiteSpace() {
  return StripMatchedCharacters(SpaceOrNewlinePredicate());
}

scoped_refptr<StringImpl> StringImpl::StripWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space) {
  return StripMatchedCharacters(UCharPredicate(is_white_space));
}

template <typename CharType>
ALWAYS_INLINE scoped_refptr<StringImpl> StringImpl::RemoveCharacters(
    base::span<const CharType> characters,
    CharacterMatchFunctionPtr find_match) {
  // Assume the common case will not remove any characters
  size_t i = 0;
  while (i < characters.size() && !find_match(characters[i])) {
    ++i;
  }
  if (i == characters.size()) {
    return this;
  }

  StringBuffer<CharType> data(characters.size());
  auto to = data.Span();
  size_t outc = i;

  if (outc) {
    to.copy_prefix_from(characters.first(outc));
  }

  while (true) {
    while (i < characters.size() && find_match(characters[i])) {
      ++i;
    }
    while (i < characters.size() && !find_match(characters[i])) {
      to[outc++] = characters[i];
      ++i;
    }
    if (i == characters.size()) {
      break;
    }
  }

  data.Shrink(outc);
  return data.Release();
}

scoped_refptr<StringImpl> StringImpl::RemoveCharacters(
    CharacterMatchFunctionPtr find_match) {
  if (Is8Bit())
    return RemoveCharacters(Span8(), find_match);
  return RemoveCharacters(Span16(), find_match);
}

scoped_refptr<StringImpl> StringImpl::Remove(wtf_size_t start,
                                             wtf_size_t length_to_remove) {
  if (length_to_remove <= 0)
    return this;
  if (start >= length_)
    return this;

  length_to_remove = std::min(length_ - start, length_to_remove);
  wtf_size_t removed_end = start + length_to_remove;

  return VisitCharacters(
      *this, [start, length_to_remove, removed_end](auto chars) {
        using CharType = decltype(chars)::value_type;
        StringBuffer<CharType> buffer(chars.size() - length_to_remove);
        auto [before, after] = buffer.Span().split_at(start);
        CopyChars(before, chars.first(start));
        CopyChars(after, chars.subspan(removed_end));
        return buffer.Release();
      });
}

template <typename CharType, class UCharPredicate>
inline scoped_refptr<StringImpl> StringImpl::SimplifyMatchedCharactersToSpace(
    base::span<const CharType> from,
    UCharPredicate predicate,
    StripBehavior strip_behavior) {
  StringBuffer<CharType> data(length_);

  size_t outc = 0;
  bool changed_to_space = false;

  auto to = data.Span();

  if (strip_behavior == kStripExtraWhiteSpace) {
    size_t i = 0;
    while (true) {
      while (i < from.size() && predicate(from[i])) {
        if (from[i] != ' ') {
          changed_to_space = true;
        }
        ++i;
      }
      while (i < from.size() && !predicate(from[i])) {
        to[outc++] = from[i++];
      }
      if (i < from.size()) {
        to[outc++] = ' ';
      } else {
        break;
      }
    }

    if (outc > 0 && to[outc - 1] == ' ')
      --outc;
  } else {
    for (size_t i = 0; i < from.size(); ++i) {
      CharType c = from[i];
      if (predicate(c)) {
        if (c != ' ') {
          changed_to_space = true;
        }
        c = ' ';
      }
      to[outc++] = c;
    }
  }

  if (outc == from.size() && !changed_to_space) {
    return this;
  }

  data.Shrink(outc);
  return data.Release();
}

scoped_refptr<StringImpl> StringImpl::SimplifyWhiteSpace(
    StripBehavior strip_behavior) {
  return VisitCharacters(*this, [&](auto chars) {
    return SimplifyMatchedCharactersToSpace(chars, SpaceOrNewlinePredicate(),
                                            strip_behavior);
  });
}

scoped_refptr<StringImpl> StringImpl::SimplifyWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space,
    StripBehavior strip_behavior) {
  return VisitCharacters(*this, [&](auto chars) {
    return SimplifyMatchedCharactersToSpace(
        chars, UCharPredicate(is_white_space), strip_behavior);
  });
}

int StringImpl::ToInt(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToInt(Span8(), options, ok);
  return CharactersToInt(Span16(), options, ok);
}

wtf_size_t StringImpl::ToUInt(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToUInt(Span8(), options, ok);
  return CharactersToUInt(Span16(), options, ok);
}

wtf_size_t StringImpl::HexToUIntStrict(bool* ok) {
  constexpr auto kStrict = NumberParsingOptions::Strict();
  if (Is8Bit()) {
    return HexCharactersToUInt(Span8(), kStrict, ok);
  }
  return HexCharactersToUInt(Span16(), kStrict, ok);
}

uint64_t StringImpl::HexToUInt64Strict(bool* ok) {
  constexpr auto kStrict = NumberParsingOptions::Strict();
  if (Is8Bit()) {
    return HexCharactersToUInt64(Span8(), kStrict, ok);
  }
  return HexCharactersToUInt64(Span16(), kStrict, ok);
}

int64_t StringImpl::ToInt64(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToInt64(Span8(), options, ok);
  return CharactersToInt64(Span16(), options, ok);
}

uint64_t StringImpl::ToUInt64(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToUInt64(Span8(), options, ok);
  return CharactersToUInt64(Span16(), options, ok);
}

double StringImpl::ToDouble(bool* ok) {
  if (Is8Bit())
    return CharactersToDouble(Span8(), ok);
  return CharactersToDouble(Span16(), ok);
}

float StringImpl::ToFloat(bool* ok) {
  if (Is8Bit())
    return CharactersToFloat(Span8(), ok);
  return CharactersToFloat(Span16(), ok);
}

// Table is based on ftp://ftp.unicode.org/Public/UNIDATA/CaseFolding.txt
const std::array<UChar, 256> StringImpl::kLatin1CaseFoldTable = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f, 0x0010, 0x0011,
    0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a,
    0x001b, 0x001c, 0x001d, 0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070,
    0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079,
    0x007a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086,
    0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098,
    0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1,
    0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x00aa,
    0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3,
    0x00b4, 0x03bc, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc,
    0x00bd, 0x00be, 0x00bf, 0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5,
    0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee,
    0x00ef, 0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00d7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00df, 0x00e0,
    0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9,
    0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2,
    0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb,
    0x00fc, 0x00fd, 0x00fe, 0x00ff,
};

bool DeprecatedEqualIgnoringCase(base::span<const LChar> a,
                                 base::span<const LChar> b) {
  CHECK_EQ(a.size(), b.size());
  size_t length = b.size();
  DCHECK_GE(length, 0u);
  const LChar* a_data = a.data();
  const LChar* b_data = b.data();
  if (a_data == b_data) {
    return true;
  }
  while (length--) {
    // SAFETY: The above `CHECK_EQ()` and `while (length--)` guarantees that
    // `a_data` moves inside `a`, and `b_data` moves inside `b`.
    if (UNSAFE_BUFFERS(StringImpl::kLatin1CaseFoldTable[*a_data++] !=
                       StringImpl::kLatin1CaseFoldTable[*b_data++])) {
      return false;
    }
  }
  return true;
}

bool DeprecatedEqualIgnoringCase(base::span<const UChar> a,
                                 base::span<const UChar> b) {
  CHECK_EQ(a.size(), b.size());
  size_t length = b.size();
  DCHECK_GE(length, 0u);
  if (a.data() == b.data()) {
    return true;
  }
  return !unicode::Umemcasecmp(a.data(), b.data(), length);
}

bool DeprecatedEqualIgnoringCase(base::span<const UChar> a,
                                 base::span<const LChar> b) {
  CHECK_EQ(a.size(), b.size());
  const UChar* a_data = a.data();
  const LChar* b_data = b.data();
  size_t length = b.size();
  while (length--) {
    // SAFETY: The above `CHECK_EQ()` and `while (length--)` guarantees that
    // `a_data` moves inside `a`, and `b_data` moves inside `b`.
    if (UNSAFE_BUFFERS(unicode::FoldCase(*a_data++) !=
                       StringImpl::kLatin1CaseFoldTable[*b_data++])) {
      return false;
    }
  }
  return true;
}

wtf_size_t StringImpl::Find(CharacterMatchFunctionPtr match_function,
                            wtf_size_t start) const {
  if (Is8Bit())
    return blink::Find(Span8(), match_function, start);
  return blink::Find(Span16(), match_function, start);
}

wtf_size_t StringImpl::Find(base::RepeatingCallback<bool(UChar)> match_callback,
                            wtf_size_t index) const {
  return VisitCharacters(*this, [&](auto chars) {
    while (index < chars.size()) {
      if (match_callback.Run(chars[index])) {
        return index;
      }
      ++index;
    }
    return kNotFound;
  });
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindInternal(
    base::span<const SearchCharacterType> search,
    base::span<const MatchCharacterType> match,
    wtf_size_t index) {
  // Optimization: keep a running hash of the strings,
  // only call equal() if the hashes match.

  wtf_size_t match_length = base::checked_cast<wtf_size_t>(match.size());
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta =
      base::checked_cast<wtf_size_t>(search.size() - match.size());

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;

  for (size_t i = 0; i < match_length; ++i) {
    search_hash += search[i];
    match_hash += match[i];
  }

  wtf_size_t i = 0;
  // Keep looping until we match.
  //
  // We don't use base::span methods for better performance.
  const SearchCharacterType* search_data = search.data();
  while (search_hash != match_hash ||
         !std::equal(match.begin(), match.end(), search_data)) {
    if (i == delta)
      return kNotFound;
    // SAFETY: This function ensures `search_data[match_length]` and
    // `search_data[0]` are safe.
    search_hash += UNSAFE_BUFFERS(search_data[match_length]);
    search_hash -= UNSAFE_BUFFERS(search_data[0]);
    ++i;
    UNSAFE_BUFFERS(++search_data);
  }
  return index + i;
}

// Optimized for the most common case where `search` and `match` are LChar.
template <>
ALWAYS_INLINE wtf_size_t FindInternal(base::span<const LChar> search,
                                      base::span<const LChar> match,
                                      wtf_size_t index) {
  CHECK_LT(1u, match.size());

  base::span<const LChar> current = search;

  while (current.size() >= match.size()) {
    base::span<const LChar> search_span =
        current.first(current.size() - match.size() + 1);

    // SAFETY: Safe because we're staying within the bounds of the span. Did not
    // use other options (such as std::find) because this is empirically faster
    // in a hot method.
    const LChar* p = UNSAFE_BUFFERS(static_cast<const LChar*>(memchr(
        search_span.data(), match[0], search_span.size() * sizeof(LChar))));
    if (!p) {
      return kNotFound;
    }

    current = current.subspan(static_cast<wtf_size_t>(p - current.data()));
    CHECK_LE(match.size(), current.size());

    // SAFETY: Safe because we're reading match.size() chars from current and
    // match and we've just CHECK'd that current is at least as long as match.
    // Did not use other options because this is empirically faster in a hot
    // method.
    if (UNSAFE_BUFFERS(memcmp(current.data(), match.data(),
                              match.size() * sizeof(LChar))) == 0) {
      return index + (p - search.data());
    }

    current = current.subspan(1u);
  }

  return kNotFound;
}

wtf_size_t StringImpl::Find(const StringView& match_string,
                            wtf_size_t index) const {
  if (match_string.IsNull()) [[unlikely]] {
    return kNotFound;
  }

  wtf_size_t match_length = match_string.length();

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return blink::Find(Span8(), match_string[0], index);
    return blink::Find(Span16(), match_string[0], index);
  }

  if (!match_length) [[unlikely]] {
    return std::min(index, length());
  }

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length) {
    return kNotFound;
  }

  if (Is8Bit()) {
    if (match_string.Is8Bit()) {
      return FindInternal(Span8().subspan(index), match_string.Span8(), index);
    }
    return FindInternal(Span8().subspan(index), match_string.Span16(), index);
  }
  if (match_string.Is8Bit()) {
    return FindInternal(Span16().subspan(index), match_string.Span8(), index);
  }
  return FindInternal(Span16().subspan(index), match_string.Span16(), index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindIgnoringCaseInternal(
    base::span<const SearchCharacterType> search,
    base::span<const MatchCharacterType> match,
    wtf_size_t index) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = search.size() - match.size();

  wtf_size_t i = 0;
  const SearchCharacterType* search_data = search.data();
  // Keep looping until we match.
  // SAFETY: The `i == delta` check below guarantees the span is in `search`.
  while (!DeprecatedEqualIgnoringCase(
      UNSAFE_BUFFERS(
          base::span(search_data + i, search_data + i + match.size())),
      match)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

wtf_size_t StringImpl::DeprecatedFindIgnoringCase(
    const StringView& match_string,
    wtf_size_t index) const {
  if (match_string.IsNull()) [[unlikely]] {
    return kNotFound;
  }

  wtf_size_t match_length = match_string.length();
  if (!match_length)
    return std::min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  return VisitCharacters(*this, [&](auto chars) {
    auto split_chars = chars.subspan(index);
    return match_string.Is8Bit()
               ? FindIgnoringCaseInternal(split_chars, match_string.Span8(),
                                          index)
               : FindIgnoringCaseInternal(split_chars, match_string.Span16(),
                                          index);
  });
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindIgnoringASCIICaseInternal(
    base::span<const SearchCharacterType> search,
    base::span<const MatchCharacterType> match,
    wtf_size_t index) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = search.size() - match.size();

  wtf_size_t i = 0;
  const SearchCharacterType* search_data = search.data();
  // Keep looping until we match.
  // SAFETY: The `i == delta` check below guarantees the span is in `search`.
  while (!EqualIgnoringASCIICase(
      UNSAFE_BUFFERS(
          base::span(search_data + i, search_data + i + match.size())),
      match)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

wtf_size_t StringImpl::FindIgnoringASCIICase(const StringView& match_string,
                                             wtf_size_t index) const {
  if (match_string.IsNull()) [[unlikely]] {
    return kNotFound;
  }

  wtf_size_t match_length = match_string.length();
  if (!match_length)
    return std::min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  return VisitCharacters(*this, [&](auto chars) {
    auto sub_span = chars.subspan(index);
    return match_string.Is8Bit() ? FindIgnoringASCIICaseInternal(
                                       sub_span, match_string.Span8(), index)
                                 : FindIgnoringASCIICaseInternal(
                                       sub_span, match_string.Span16(), index);
  });
}

wtf_size_t StringImpl::ReverseFind(UChar c, wtf_size_t index) const {
  if (Is8Bit())
    return blink::ReverseFind(Span8(), c, index);
  return blink::ReverseFind(Span16(), c, index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t ReverseFindInternal(
    base::span<const SearchCharacterType> search,
    base::span<const MatchCharacterType> match,
    wtf_size_t index) {
  // Optimization: keep a running hash of the strings,
  // only call equal if the hashes match.

  wtf_size_t match_length = base::checked_cast<wtf_size_t>(match.size());
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = std::min(
      index, base::checked_cast<wtf_size_t>(search.size() - match_length));

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;
  for (wtf_size_t i = 0; i < match_length; ++i) {
    search_hash += search[delta + i];
    match_hash += match[i];
  }

  // Keep looping until we match.
  //
  // We don't use base::span methods for better performance.
  // SAFETY: This function ensures `search.data() + delta` and
  // `search.data() + delta + match_length` are safe.
  const SearchCharacterType* search_data =
      UNSAFE_BUFFERS(search.data() + delta);
  while (search_hash != match_hash ||
         !std::equal(match.begin(), match.end(), search_data)) {
    if (!delta)
      return kNotFound;
    --delta;
    UNSAFE_BUFFERS(--search_data);
    search_hash -= UNSAFE_BUFFERS(search_data[match_length]);
    search_hash += UNSAFE_BUFFERS(search_data[0]);
  }
  return delta;
}

wtf_size_t StringImpl::ReverseFind(const StringView& match_string,
                                   wtf_size_t index) const {
  if (match_string.IsNull()) [[unlikely]] {
    return kNotFound;
  }

  wtf_size_t match_length = match_string.length();
  wtf_size_t our_length = length();
  if (!match_length)
    return std::min(index, our_length);

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return blink::ReverseFind(Span8(), match_string[0], index);
    return blink::ReverseFind(Span16(), match_string[0], index);
  }

  // Check index & matchLength are in range.
  if (match_length > our_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return ReverseFindInternal(Span8(), match_string.Span8(), index);
    return ReverseFindInternal(Span8(), match_string.Span16(), index);
  }
  if (match_string.Is8Bit())
    return ReverseFindInternal(Span16(), match_string.Span8(), index);
  return ReverseFindInternal(Span16(), match_string.Span16(), index);
}

bool StringImpl::StartsWith(UChar character) const {
  return length_ && (*this)[0] == character;
}

bool StringImpl::StartsWith(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    auto span = Span8().first(prefix.length());
    return prefix.Is8Bit() ? span == prefix.Span8() : span == prefix.Span16();
  }
  auto span = Span16().first(prefix.length());
  return prefix.Is8Bit() ? span == prefix.Span8() : span == prefix.Span16();
}

bool StringImpl::DeprecatedStartsWithIgnoringCase(
    const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  return VisitCharacters(*this, [&prefix](auto chars) {
    auto split_chars = chars.first(prefix.length());
    return prefix.Is8Bit()
               ? DeprecatedEqualIgnoringCase(split_chars, prefix.Span8())
               : DeprecatedEqualIgnoringCase(split_chars, prefix.Span16());
  });
}

bool StringImpl::StartsWithIgnoringCaseAndAccents(
    const StringView& prefix) const {
  std::u16string s = ToU16String();
  std::u16string p = blink::ToU16String(prefix);
  size_t match_index = 1U;

  if (base::i18n::StringSearchIgnoringCaseAndAccents(
          p, s, &match_index,
          /*match_length=*/nullptr)) {
    return match_index == 0U;
  }

  return false;
}

std::u16string StringImpl::ToU16String() const {
  return blink::ToU16String(StringView(*this));
}

bool StringImpl::StartsWithIgnoringASCIICase(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  return VisitCharacters(*this, [&prefix](auto chars) {
    auto sub_span = chars.first(prefix.length());
    return prefix.Is8Bit() ? EqualIgnoringASCIICase(sub_span, prefix.Span8())
                           : EqualIgnoringASCIICase(sub_span, prefix.Span16());
  });
}

bool StringImpl::EndsWith(UChar character) const {
  return length_ && (*this)[length_ - 1] == character;
}

bool StringImpl::EndsWith(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  if (Is8Bit()) {
    auto span = Span8().last(suffix.length());
    return suffix.Is8Bit() ? span == suffix.Span8() : span == suffix.Span16();
  }
  auto span = Span16().last(suffix.length());
  return suffix.Is8Bit() ? span == suffix.Span8() : span == suffix.Span16();
}

bool StringImpl::DeprecatedEndsWithIgnoringCase(
    const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  wtf_size_t start_offset = length() - suffix.length();
  return VisitCharacters(*this, [&](auto chars) {
    auto split_chars = chars.subspan(start_offset);
    return suffix.Is8Bit()
               ? DeprecatedEqualIgnoringCase(split_chars, suffix.Span8())
               : DeprecatedEqualIgnoringCase(split_chars, suffix.Span16());
  });
}

bool StringImpl::EndsWithIgnoringASCIICase(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  wtf_size_t start_offset = length() - suffix.length();
  return VisitCharacters(*this, [&](auto chars) {
    auto sub_span = chars.subspan(start_offset);
    return suffix.Is8Bit() ? EqualIgnoringASCIICase(sub_span, suffix.Span8())
                           : EqualIgnoringASCIICase(sub_span, suffix.Span16());
  });
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar old_c, UChar new_c) {
  if (old_c == new_c)
    return this;

  if (Find(old_c) == kNotFound)
    return this;

  if (Is8Bit()) {
    if (new_c <= 0xff) {
      base::span<LChar> data8;
      scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data8);
      CopyAndReplace(data8, Span8(), static_cast<LChar>(old_c),
                     static_cast<LChar>(new_c));
      return new_impl;
    }

    // There is the possibility we need to up convert from 8 to 16 bit,
    // create a 16 bit string for the result.
    base::span<UChar> data16;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data16);
    CopyAndReplace(data16, Span8(), old_c, new_c);
    return new_impl;
  }

  base::span<UChar> data16;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data16);
  CopyAndReplace(data16, Span16(), old_c, new_c);
  return new_impl;
}

// TODO(esprehn): Passing a null replacement is the same as empty string for
// this method but all others treat null as a no-op. We should choose one
// behavior.
scoped_refptr<StringImpl> StringImpl::Replace(wtf_size_t position,
                                              wtf_size_t length_to_replace,
                                              const StringView& string) {
  position = std::min(position, length());
  length_to_replace = std::min(length_to_replace, length() - position);
  if (!length_to_replace && string.empty()) {
    return this;
  }

  const wtf_size_t new_length = ComputeSizeAfterReplacement(
      length(), 1, length_to_replace, string.length());

  if (Is8Bit() && (string.IsNull() || string.Is8Bit())) {
    const base::span<const LChar> source8 = Span8();
    base::span<LChar> data8;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_length, data8);

    data8.take_first(position).copy_from(source8.first(position));
    auto data8_replaced = data8.take_first(string.length());
    if (!string.IsNull()) {
      data8_replaced.copy_from(string.Span8());
    }
    data8.copy_from(source8.subspan(position + length_to_replace));
    return new_impl;
  }

  base::span<UChar> data16;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_length, data16);

  CopyStringFragment(StringView(*this, 0, position),
                     data16.take_first(position));
  auto data16_replaced = data16.take_first(string.length());
  if (!string.IsNull()) {
    CopyStringFragment(string, data16_replaced);
  }
  CopyStringFragment(StringView(*this, position + length_to_replace), data16);
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar pattern,
                                              const StringView& replacement) {
  if (replacement.IsNull())
    return this;

  // Count the matches.
  wtf_size_t match_count = 0;
  wtf_size_t search_index = 0;
  while ((search_index = Find(pattern, search_index)) != kNotFound) {
    ++match_count;
    ++search_index;
  }

  // If we have 0 matches then we don't have to do any more work.
  if (!match_count) {
    return this;
  }

  // Construct the new data.
  const wtf_size_t new_size = ComputeSizeAfterReplacement(
      length_, match_count, 1, replacement.length());

  if (Is8Bit() && replacement.Is8Bit()) {
    base::span<LChar> data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
    DoReplace(Span8(), pattern, replacement.Span8(), data);
    return new_impl;
  }

  base::span<UChar> data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
  if (replacement.Is8Bit()) {
    DoReplace(Span16(), pattern, replacement.Span8(), data);
  } else {
    if (Is8Bit()) {
      DoReplace(Span8(), pattern, replacement.Span16(), data);
    } else {
      DoReplace(Span16(), pattern, replacement.Span16(), data);
    }
  }
  return new_impl;
}

template <typename DestCharType,
          typename SrcCharType,
          typename ReplacementCharType>
void StringImpl::DoReplace(base::span<const SrcCharType> src,
                           UChar pattern,
                           base::span<const ReplacementCharType> replacement,
                           base::span<DestCharType> dest) const {
  wtf_size_t src_segment_end;
  wtf_size_t src_segment_start = 0;
  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    auto src_before =
        src.subspan(src_segment_start, src_segment_end - src_segment_start);

    CopyChars(dest.take_first(src_before.size()), src_before);
    CopyChars(dest.take_first(replacement.size()), replacement);

    src_segment_start = src_segment_end + 1;
  }

  CopyChars(dest, src.subspan(src_segment_start));
}

scoped_refptr<StringImpl> StringImpl::Replace(const StringView& pattern,
                                              const StringView& replacement) {
  if (pattern.IsNull() || replacement.IsNull())
    return this;

  if (pattern.empty()) {
    return this;
  }

  // Count the matches.
  wtf_size_t match_count = 0;
  wtf_size_t search_index = 0;
  while ((search_index = Find(pattern, search_index)) != kNotFound) {
    ++match_count;
    search_index += pattern.length();
  }

  // If we have 0 matches, we don't have to do any more work
  if (!match_count)
    return this;

  // Construct the new data.
  const wtf_size_t new_size = ComputeSizeAfterReplacement(
      length_, match_count, pattern.length(), replacement.length());

  // There are 4 cases:
  // 1. This and replacement are both 8 bit.
  // 2. This and replacement are both 16 bit.
  // 3. This is 8 bit and replacement is 16 bit.
  // 4. This is 16 bit and replacement is 8 bit.
  if (Is8Bit() && replacement.Is8Bit()) {
    // Case 1
    base::span<LChar> data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
    DoReplace(pattern, replacement, data);
    return new_impl;
  }

  // Case 2, 3 and 4
  base::span<UChar> data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
  DoReplace(pattern, replacement, data);
  return new_impl;
}

template <typename DestCharType>
void StringImpl::DoReplace(const StringView& pattern,
                           const StringView& replacement,
                           base::span<DestCharType> dest) const {
  wtf_size_t src_segment_end;
  wtf_size_t src_segment_start = 0;
  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    const StringView source_before(*this, src_segment_start,
                                   src_segment_end - src_segment_start);

    CopyStringFragment(source_before, dest.take_first(source_before.length()));
    CopyStringFragment(replacement, dest.take_first(replacement.length()));

    src_segment_start = src_segment_end + pattern.length();
  }

  CopyStringFragment(StringView(*this, src_segment_start), dest);
}

scoped_refptr<StringImpl> StringImpl::UpconvertedString() {
  if (Is8Bit())
    return String::Make16BitFrom8BitSource(Span8()).ReleaseImpl();
  return this;
}

static inline bool StringImplContentEqual(const StringImpl* a,
                                          const StringImpl* b) {
  wtf_size_t a_length = a->length();
  wtf_size_t b_length = b->length();
  if (a_length != b_length)
    return false;

  if (!a_length)
    return true;

  return VisitCharacters(*a, [b](auto chars) {
    return b->Is8Bit() ? chars == b->Span8() : chars == b->Span16();
  });
}

bool Equal(const StringImpl* a, const StringImpl* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  if (a->IsAtomic() && b->IsAtomic())
    return false;

  return StringImplContentEqual(a, b);
}

template <typename CharType>
inline bool EqualInternal(const StringImpl* a, base::span<const CharType> b) {
  if (!a)
    return !b.data();
  if (!b.data()) {
    return false;
  }

  if (a->length() != b.size()) {
    return false;
  }
  return a->Is8Bit() ? a->Span8() == b : a->Span16() == b;
}

bool Equal(const StringImpl* a, base::span<const LChar> b) {
  return EqualInternal(a, b);
}

bool Equal(const StringImpl* a, base::span<const UChar> b) {
  return EqualInternal(a, b);
}

// SAFETY: Safe only when latin1 is null-terminated cstring.
template <typename StringType>
UNSAFE_BUFFER_USAGE bool EqualToCString(const StringType* a, const LChar* b) {
  DCHECK(b);
  return VisitCharacters(*a, [b](auto chars) {
    for (wtf_size_t i = 0; auto ac : chars) {
      LChar bc = b[i++];
      if (!bc || ac != bc) {
        return false;
      }
    }
    return !b[chars.size()];
  });
}

// SAFETY: Safe only when latin1 is null-terminated cstring.
UNSAFE_BUFFER_USAGE bool EqualToCString(const StringImpl* a,
                                        const char* latin1) {
  if (!a) {
    return !latin1;
  }
  return EqualToCString(a, reinterpret_cast<const LChar*>(latin1));
}

// SAFETY: Safe only when latin1 is null-terminated cstring.
UNSAFE_BUFFER_USAGE bool EqualToCString(const StringView& a,
                                        const char* latin1) {
  return EqualToCString(&a, reinterpret_cast<const LChar*>(latin1));
}

bool EqualNonNull(const StringImpl* a, const StringImpl* b) {
  DCHECK(a);
  DCHECK(b);
  if (a == b)
    return true;

  return StringImplContentEqual(a, b);
}

bool EqualIgnoringNullity(StringImpl* a, StringImpl* b) {
  if (!a && b && !b->length())
    return true;
  if (!b && a && !a->length())
    return true;
  return Equal(a, b);
}

template <typename CharacterType>
int CodeUnitCompareIgnoringASCIICase(const StringImpl* string1,
                                     base::span<const CharacterType> string2) {
  if (!string1) {
    return !string2.empty() ? -1 : 0;
  }
  return VisitCharacters(*string1, [string2](auto string1_chars) {
    return CodeUnitCompareIgnoringAsciiCase(string1_chars, string2);
  });
}

int CodeUnitCompareIgnoringASCIICase(const StringImpl* string1,
                                     const LChar* string2) {
  if (!string2) {
    return string1 && string1->length() ? 1 : 0;
  }
  std::string_view string2_view(reinterpret_cast<const char*>(string2));
  return CodeUnitCompareIgnoringASCIICase(string1, base::span(string2_view));
}

int CodeUnitCompareIgnoringASCIICase(const StringImpl* string1,
                                     const StringImpl* string2) {
  if (!string2) {
    return string1 && string1->length() ? 1 : 0;
  }
  return VisitCharacters(*string2, [string1](auto string2_chars) {
    return CodeUnitCompareIgnoringASCIICase(string1, string2_chars);
  });
}

}  // namespace blink
