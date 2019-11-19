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
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

using std::numeric_limits;

namespace WTF {

// As of Jan 2017, StringImpl needs 2 * sizeof(int) + 29 bits of data, and
// sizeof(ThreadRestrictionVerifier) is 16 bytes. Thus, in DCHECK mode the
// class may be padded to 32 bytes.
#if DCHECK_IS_ON()
static_assert(sizeof(StringImpl) <= 8 * sizeof(int),
              "StringImpl should stay small");
#else
static_assert(sizeof(StringImpl) <= 3 * sizeof(int),
              "StringImpl should stay small");
#endif

void* StringImpl::operator new(size_t size) {
  DCHECK_EQ(size, sizeof(StringImpl));
  return Partitions::BufferMalloc(size, "WTF::StringImpl");
}

void StringImpl::operator delete(void* ptr) {
  Partitions::BufferFree(ptr);
}

inline StringImpl::~StringImpl() {
  DCHECK(!IsStatic());

  if (IsAtomic())
    AtomicStringTable::Instance().Remove(this);
}

void StringImpl::DestroyIfNotStatic() const {
  if (!IsStatic())
    delete this;
}

void StringImpl::UpdateContainsOnlyASCIIOrEmpty() const {
  contains_only_ascii_ = Is8Bit()
                             ? CharactersAreAllASCII(Characters8(), length())
                             : CharactersAreAllASCII(Characters16(), length());
  needs_ascii_check_ = false;
}

bool StringImpl::IsSafeToSendToAnotherThread() const {
  if (IsStatic())
    return true;
  // AtomicStrings are not safe to send between threads as ~StringImpl()
  // will try to remove them from the wrong AtomicStringTable.
  if (IsAtomic())
    return false;
  if (HasOneRef())
    return true;
  return false;
}

#if DCHECK_IS_ON()
std::string StringImpl::AsciiForDebugging() const {
  return String(IsolatedCopy()->Substring(0, 128)).Ascii();
}
#endif

scoped_refptr<StringImpl> StringImpl::CreateUninitialized(wtf_size_t length,
                                                          LChar*& data) {
  if (!length) {
    data = nullptr;
    return empty_;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = static_cast<StringImpl*>(Partitions::BufferMalloc(
      AllocationSize<LChar>(length), "WTF::StringImpl"));

  data = reinterpret_cast<LChar*>(string + 1);
  return base::AdoptRef(new (string) StringImpl(length, kForce8BitConstructor));
}

scoped_refptr<StringImpl> StringImpl::CreateUninitialized(wtf_size_t length,
                                                          UChar*& data) {
  if (!length) {
    data = nullptr;
    return empty_;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = static_cast<StringImpl*>(Partitions::BufferMalloc(
      AllocationSize<UChar>(length), "WTF::StringImpl"));

  data = reinterpret_cast<UChar*>(string + 1);
  return base::AdoptRef(new (string) StringImpl(length));
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

DEFINE_GLOBAL(StringImpl, g_global_empty);
DEFINE_GLOBAL(StringImpl, g_global_empty16_bit);
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

StringImpl* StringImpl::CreateStatic(const char* string,
                                     wtf_size_t length,
                                     wtf_size_t hash) {
#if DCHECK_IS_ON()
  DCHECK(g_allow_creation_of_static_strings);
#endif
  DCHECK(string);
  DCHECK(length);

  StaticStringsTable::const_iterator it = StaticStrings().find(hash);
  if (it != StaticStrings().end()) {
    DCHECK(!memcmp(string, it->value + 1, length * sizeof(LChar)));
    return it->value;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  CHECK_LE(length,
           ((std::numeric_limits<wtf_size_t>::max() - sizeof(StringImpl)) /
            sizeof(LChar)));
  wtf_size_t size = sizeof(StringImpl) + length * sizeof(LChar);

  WTF_INTERNAL_LEAK_SANITIZER_DISABLED_SCOPE;
  StringImpl* impl = static_cast<StringImpl*>(
      Partitions::BufferMalloc(size, "WTF::StringImpl"));

  LChar* data = reinterpret_cast<LChar*>(impl + 1);
  impl = new (impl) StringImpl(length, hash, kStaticString);
  memcpy(data, string, length * sizeof(LChar));
#if DCHECK_IS_ON()
  impl->AssertHashIsCorrect();
#endif

  DCHECK(IsMainThread());
  highest_static_string_length_ =
      std::max(highest_static_string_length_, length);
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

scoped_refptr<StringImpl> StringImpl::Create(const UChar* characters,
                                             wtf_size_t length) {
  if (!characters || !length)
    return empty_;

  UChar* data;
  scoped_refptr<StringImpl> string = CreateUninitialized(length, data);
  memcpy(data, characters, length * sizeof(UChar));
  return string;
}

scoped_refptr<StringImpl> StringImpl::Create(const LChar* characters,
                                             wtf_size_t length) {
  if (!characters || !length)
    return empty_;

  LChar* data;
  scoped_refptr<StringImpl> string = CreateUninitialized(length, data);
  memcpy(data, characters, length * sizeof(LChar));
  return string;
}

scoped_refptr<StringImpl> StringImpl::Create8BitIfPossible(
    const UChar* characters,
    wtf_size_t length) {
  if (!characters || !length)
    return empty_;

  LChar* data;
  scoped_refptr<StringImpl> string = CreateUninitialized(length, data);

  for (wtf_size_t i = 0; i < length; ++i) {
    if (characters[i] & 0xff00)
      return Create(characters, length);
    data[i] = static_cast<LChar>(characters[i]);
  }

  return string;
}

scoped_refptr<StringImpl> StringImpl::Create(const LChar* string) {
  if (!string)
    return empty_;
  size_t length = strlen(reinterpret_cast<const char*>(string));
  return Create(string, SafeCast<wtf_size_t>(length));
}

bool StringImpl::ContainsOnlyWhitespaceOrEmpty() {
  // FIXME: The definition of whitespace here includes a number of characters
  // that are not whitespace from the point of view of LayoutText; I wonder if
  // that's a problem in practice.
  if (Is8Bit()) {
    for (wtf_size_t i = 0; i < length_; ++i) {
      UChar c = Characters8()[i];
      if (!IsASCIISpace(c))
        return false;
    }

    return true;
  }

  for (wtf_size_t i = 0; i < length_; ++i) {
    UChar c = Characters16()[i];
    if (!IsASCIISpace(c))
      return false;
  }
  return true;
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
    return Create(Characters8() + start, length);

  return Create(Characters16() + start, length);
}

UChar32 StringImpl::CharacterStartingAt(wtf_size_t i) {
  if (Is8Bit())
    return Characters8()[i];
  if (U16_IS_SINGLE(Characters16()[i]))
    return Characters16()[i];
  if (i + 1 < length_ && U16_IS_LEAD(Characters16()[i]) &&
      U16_IS_TRAIL(Characters16()[i + 1]))
    return U16_GET_SUPPLEMENTARY(Characters16()[i], Characters16()[i + 1]);
  return 0;
}

wtf_size_t StringImpl::CopyTo(UChar* buffer,
                              wtf_size_t start,
                              wtf_size_t max_length) const {
  wtf_size_t number_of_characters_to_copy =
      std::min(length() - start, max_length);
  if (!number_of_characters_to_copy)
    return 0;
  if (Is8Bit())
    CopyChars(buffer, Characters8() + start, number_of_characters_to_copy);
  else
    CopyChars(buffer, Characters16() + start, number_of_characters_to_copy);
  return number_of_characters_to_copy;
}

scoped_refptr<StringImpl> StringImpl::LowerASCII() {
  // First scan the string for uppercase and non-ASCII characters:
  if (Is8Bit()) {
    wtf_size_t first_index_to_be_lowered = length_;
    for (wtf_size_t i = 0; i < length_; ++i) {
      LChar ch = Characters8()[i];
      if (IsASCIIUpper(ch)) {
        first_index_to_be_lowered = i;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_be_lowered == length_) {
      return this;
    }

    LChar* data8;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data8);
    memcpy(data8, Characters8(), first_index_to_be_lowered);

    for (wtf_size_t i = first_index_to_be_lowered; i < length_; ++i) {
      LChar ch = Characters8()[i];
      data8[i] = IsASCIIUpper(ch) ? ToASCIILower(ch) : ch;
    }
    return new_impl;
  }
  bool no_upper = true;
  UChar ored = 0;

  const UChar* end = Characters16() + length_;
  for (const UChar* chp = Characters16(); chp != end; ++chp) {
    if (IsASCIIUpper(*chp))
      no_upper = false;
    ored |= *chp;
  }
  // Nothing to do if the string is all ASCII with no uppercase.
  if (no_upper && !(ored & ~0x7F))
    return this;

  CHECK_LE(length_, static_cast<wtf_size_t>(numeric_limits<wtf_size_t>::max()));
  wtf_size_t length = length_;

  UChar* data16;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  for (wtf_size_t i = 0; i < length; ++i) {
    UChar c = Characters16()[i];
    data16[i] = IsASCIIUpper(c) ? ToASCIILower(c) : c;
  }
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::UpperASCII() {
  if (Is8Bit()) {
    LChar* data8;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data8);

    for (wtf_size_t i = 0; i < length_; ++i) {
      LChar c = Characters8()[i];
      data8[i] = IsASCIILower(c) ? ToASCIIUpper(c) : c;
    }
    return new_impl;
  }

  UChar* data16;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  for (wtf_size_t i = 0; i < length_; ++i) {
    UChar c = Characters16()[i];
    data16[i] = IsASCIILower(c) ? ToASCIIUpper(c) : c;
  }
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Fill(UChar character) {
  if (!(character & ~0x7F)) {
    LChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
    for (wtf_size_t i = 0; i < length_; ++i)
      data[i] = static_cast<LChar>(character);
    return new_impl;
  }
  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
  for (wtf_size_t i = 0; i < length_; ++i)
    data[i] = character;
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::FoldCase() {
  CHECK_LE(length_, static_cast<wtf_size_t>(numeric_limits<int32_t>::max()));
  int32_t length = length_;

  if (Is8Bit()) {
    // Do a faster loop for the case where all the characters are ASCII.
    LChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
    LChar ored = 0;

    for (int32_t i = 0; i < length; ++i) {
      LChar c = Characters8()[i];
      data[i] = ToASCIILower(c);
      ored |= c;
    }

    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    for (int32_t i = 0; i < length; ++i)
      data[i] = static_cast<LChar>(unicode::ToLower(Characters8()[i]));

    return new_impl;
  }

  // Do a faster loop for the case where all the characters are ASCII.
  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);
  UChar ored = 0;
  for (int32_t i = 0; i < length; ++i) {
    UChar c = Characters16()[i];
    ored |= c;
    data[i] = ToASCIILower(c);
  }
  if (!(ored & ~0x7F))
    return new_impl;

  // Do a slower implementation for cases that include non-ASCII characters.
  bool error;
  int32_t real_length =
      unicode::FoldCase(data, length, Characters16(), length_, &error);
  if (!error && real_length == length)
    return new_impl;
  new_impl = CreateUninitialized(real_length, data);
  unicode::FoldCase(data, real_length, Characters16(), length_, &error);
  if (error)
    return this;
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Truncate(wtf_size_t length) {
  if (length >= length_)
    return this;
  if (Is8Bit())
    return Create(Characters8(), length);
  return Create(Characters16(), length);
}

template <class UCharPredicate>
inline scoped_refptr<StringImpl> StringImpl::StripMatchedCharacters(
    UCharPredicate predicate) {
  if (!length_)
    return empty_;

  wtf_size_t start = 0;
  wtf_size_t end = length_ - 1;

  // skip white space from start
  while (start <= end &&
         predicate(Is8Bit() ? Characters8()[start] : Characters16()[start]))
    ++start;

  // only white space
  if (start > end)
    return empty_;

  // skip white space from end
  while (end && predicate(Is8Bit() ? Characters8()[end] : Characters16()[end]))
    --end;

  if (!start && end == length_ - 1)
    return this;
  if (Is8Bit())
    return Create(Characters8() + start, end + 1 - start);
  return Create(Characters16() + start, end + 1 - start);
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
  inline bool operator()(UChar ch) const { return IsSpaceOrNewline(ch); }
};

scoped_refptr<StringImpl> StringImpl::StripWhiteSpace() {
  return StripMatchedCharacters(SpaceOrNewlinePredicate());
}

scoped_refptr<StringImpl> StringImpl::StripWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space) {
  return StripMatchedCharacters(UCharPredicate(is_white_space));
}

template <typename CharType>
ALWAYS_INLINE scoped_refptr<StringImpl> StringImpl::RemoveCharacters(
    const CharType* characters,
    CharacterMatchFunctionPtr find_match) {
  const CharType* from = characters;
  const CharType* fromend = from + length_;

  // Assume the common case will not remove any characters
  while (from != fromend && !find_match(*from))
    ++from;
  if (from == fromend)
    return this;

  StringBuffer<CharType> data(length_);
  CharType* to = data.Characters();
  wtf_size_t outc = static_cast<wtf_size_t>(from - characters);

  if (outc)
    memcpy(to, characters, outc * sizeof(CharType));

  while (true) {
    while (from != fromend && find_match(*from))
      ++from;
    while (from != fromend && !find_match(*from))
      to[outc++] = *from++;
    if (from == fromend)
      break;
  }

  data.Shrink(outc);

  return data.Release();
}

scoped_refptr<StringImpl> StringImpl::RemoveCharacters(
    CharacterMatchFunctionPtr find_match) {
  if (Is8Bit())
    return RemoveCharacters(Characters8(), find_match);
  return RemoveCharacters(Characters16(), find_match);
}

scoped_refptr<StringImpl> StringImpl::Remove(wtf_size_t start,
                                             wtf_size_t length_to_remove) {
  if (length_to_remove <= 0)
    return this;
  if (start >= length_)
    return this;

  length_to_remove = std::min(length_ - start, length_to_remove);
  wtf_size_t removed_end = start + length_to_remove;

  if (Is8Bit()) {
    StringBuffer<LChar> buffer(length_ - length_to_remove);
    CopyChars(buffer.Characters(), Characters8(), start);
    CopyChars(buffer.Characters() + start, Characters8() + removed_end,
              length_ - removed_end);
    return buffer.Release();
  }
  StringBuffer<UChar> buffer(length_ - length_to_remove);
  CopyChars(buffer.Characters(), Characters16(), start);
  CopyChars(buffer.Characters() + start, Characters16() + removed_end,
            length_ - removed_end);
  return buffer.Release();
}

template <typename CharType, class UCharPredicate>
inline scoped_refptr<StringImpl> StringImpl::SimplifyMatchedCharactersToSpace(
    UCharPredicate predicate,
    StripBehavior strip_behavior) {
  StringBuffer<CharType> data(length_);

  const CharType* from = GetCharacters<CharType>();
  const CharType* fromend = from + length_;
  int outc = 0;
  bool changed_to_space = false;

  CharType* to = data.Characters();

  if (strip_behavior == kStripExtraWhiteSpace) {
    while (true) {
      while (from != fromend && predicate(*from)) {
        if (*from != ' ')
          changed_to_space = true;
        ++from;
      }
      while (from != fromend && !predicate(*from))
        to[outc++] = *from++;
      if (from != fromend)
        to[outc++] = ' ';
      else
        break;
    }

    if (outc > 0 && to[outc - 1] == ' ')
      --outc;
  } else {
    for (; from != fromend; ++from) {
      if (predicate(*from)) {
        if (*from != ' ')
          changed_to_space = true;
        to[outc++] = ' ';
      } else {
        to[outc++] = *from;
      }
    }
  }

  if (static_cast<wtf_size_t>(outc) == length_ && !changed_to_space)
    return this;

  data.Shrink(outc);

  return data.Release();
}

scoped_refptr<StringImpl> StringImpl::SimplifyWhiteSpace(
    StripBehavior strip_behavior) {
  if (Is8Bit())
    return StringImpl::SimplifyMatchedCharactersToSpace<LChar>(
        SpaceOrNewlinePredicate(), strip_behavior);
  return StringImpl::SimplifyMatchedCharactersToSpace<UChar>(
      SpaceOrNewlinePredicate(), strip_behavior);
}

scoped_refptr<StringImpl> StringImpl::SimplifyWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space,
    StripBehavior strip_behavior) {
  if (Is8Bit())
    return StringImpl::SimplifyMatchedCharactersToSpace<LChar>(
        UCharPredicate(is_white_space), strip_behavior);
  return StringImpl::SimplifyMatchedCharactersToSpace<UChar>(
      UCharPredicate(is_white_space), strip_behavior);
}

int StringImpl::ToInt(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToInt(Characters8(), length_, options, ok);
  return CharactersToInt(Characters16(), length_, options, ok);
}

wtf_size_t StringImpl::ToUInt(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToUInt(Characters8(), length_, options, ok);
  return CharactersToUInt(Characters16(), length_, options, ok);
}

wtf_size_t StringImpl::HexToUIntStrict(bool* ok) {
  if (Is8Bit()) {
    return HexCharactersToUInt(Characters8(), length_,
                               NumberParsingOptions::kStrict, ok);
  }
  return HexCharactersToUInt(Characters16(), length_,
                             NumberParsingOptions::kStrict, ok);
}

int64_t StringImpl::ToInt64(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToInt64(Characters8(), length_, options, ok);
  return CharactersToInt64(Characters16(), length_, options, ok);
}

uint64_t StringImpl::ToUInt64(NumberParsingOptions options, bool* ok) const {
  if (Is8Bit())
    return CharactersToUInt64(Characters8(), length_, options, ok);
  return CharactersToUInt64(Characters16(), length_, options, ok);
}

double StringImpl::ToDouble(bool* ok) {
  if (Is8Bit())
    return CharactersToDouble(Characters8(), length_, ok);
  return CharactersToDouble(Characters16(), length_, ok);
}

float StringImpl::ToFloat(bool* ok) {
  if (Is8Bit())
    return CharactersToFloat(Characters8(), length_, ok);
  return CharactersToFloat(Characters16(), length_, ok);
}

// Table is based on ftp://ftp.unicode.org/Public/UNIDATA/CaseFolding.txt
const UChar StringImpl::kLatin1CaseFoldTable[256] = {
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

bool DeprecatedEqualIgnoringCase(const LChar* a,
                                 const LChar* b,
                                 wtf_size_t length) {
  DCHECK_GE(length, 0u);
  if (a == b)
    return true;
  while (length--) {
    if (StringImpl::kLatin1CaseFoldTable[*a++] !=
        StringImpl::kLatin1CaseFoldTable[*b++])
      return false;
  }
  return true;
}

bool DeprecatedEqualIgnoringCase(const UChar* a,
                                 const UChar* b,
                                 wtf_size_t length) {
  DCHECK_GE(length, 0u);
  if (a == b)
    return true;
  return !unicode::Umemcasecmp(a, b, length);
}

bool DeprecatedEqualIgnoringCase(const UChar* a,
                                 const LChar* b,
                                 wtf_size_t length) {
  while (length--) {
    if (unicode::FoldCase(*a++) != StringImpl::kLatin1CaseFoldTable[*b++])
      return false;
  }
  return true;
}

wtf_size_t StringImpl::Find(CharacterMatchFunctionPtr match_function,
                            wtf_size_t start) {
  if (Is8Bit())
    return WTF::Find(Characters8(), length_, match_function, start);
  return WTF::Find(Characters16(), length_, match_function, start);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    wtf_size_t index,
    wtf_size_t search_length,
    wtf_size_t match_length) {
  // Optimization: keep a running hash of the strings,
  // only call equal() if the hashes match.

  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = search_length - match_length;

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;

  for (wtf_size_t i = 0; i < match_length; ++i) {
    search_hash += search_characters[i];
    match_hash += match_characters[i];
  }

  wtf_size_t i = 0;
  // keep looping until we match
  while (search_hash != match_hash ||
         !Equal(search_characters + i, match_characters, match_length)) {
    if (i == delta)
      return kNotFound;
    search_hash += search_characters[i + match_length];
    search_hash -= search_characters[i];
    ++i;
  }
  return index + i;
}

wtf_size_t StringImpl::Find(const StringView& match_string, wtf_size_t index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  wtf_size_t match_length = match_string.length();

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return WTF::Find(Characters8(), length(), match_string[0], index);
    return WTF::Find(Characters16(), length(), match_string[0], index);
  }

  if (UNLIKELY(!match_length))
    return std::min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindInternal(Characters8() + index, match_string.Characters8(),
                          index, search_length, match_length);
    return FindInternal(Characters8() + index, match_string.Characters16(),
                        index, search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindInternal(Characters16() + index, match_string.Characters8(),
                        index, search_length, match_length);
  return FindInternal(Characters16() + index, match_string.Characters16(),
                      index, search_length, match_length);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindIgnoringCaseInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    wtf_size_t index,
    wtf_size_t search_length,
    wtf_size_t match_length) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = search_length - match_length;

  wtf_size_t i = 0;
  // keep looping until we match
  while (!DeprecatedEqualIgnoringCase(search_characters + i, match_characters,
                                      match_length)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

wtf_size_t StringImpl::FindIgnoringCase(const StringView& match_string,
                                        wtf_size_t index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  wtf_size_t match_length = match_string.length();
  if (!match_length)
    return std::min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindIgnoringCaseInternal(Characters8() + index,
                                      match_string.Characters8(), index,
                                      search_length, match_length);
    return FindIgnoringCaseInternal(Characters8() + index,
                                    match_string.Characters16(), index,
                                    search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindIgnoringCaseInternal(Characters16() + index,
                                    match_string.Characters8(), index,
                                    search_length, match_length);
  return FindIgnoringCaseInternal(Characters16() + index,
                                  match_string.Characters16(), index,
                                  search_length, match_length);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t FindIgnoringASCIICaseInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    wtf_size_t index,
    wtf_size_t search_length,
    wtf_size_t match_length) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = search_length - match_length;

  wtf_size_t i = 0;
  // keep looping until we match
  while (!EqualIgnoringASCIICase(search_characters + i, match_characters,
                                 match_length)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

wtf_size_t StringImpl::FindIgnoringASCIICase(const StringView& match_string,
                                             wtf_size_t index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  wtf_size_t match_length = match_string.length();
  if (!match_length)
    return std::min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  wtf_size_t search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindIgnoringASCIICaseInternal(Characters8() + index,
                                           match_string.Characters8(), index,
                                           search_length, match_length);
    return FindIgnoringASCIICaseInternal(Characters8() + index,
                                         match_string.Characters16(), index,
                                         search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindIgnoringASCIICaseInternal(Characters16() + index,
                                         match_string.Characters8(), index,
                                         search_length, match_length);
  return FindIgnoringASCIICaseInternal(Characters16() + index,
                                       match_string.Characters16(), index,
                                       search_length, match_length);
}

wtf_size_t StringImpl::ReverseFind(UChar c, wtf_size_t index) {
  if (Is8Bit())
    return WTF::ReverseFind(Characters8(), length_, c, index);
  return WTF::ReverseFind(Characters16(), length_, c, index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static wtf_size_t ReverseFindInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    wtf_size_t index,
    wtf_size_t length,
    wtf_size_t match_length) {
  // Optimization: keep a running hash of the strings,
  // only call equal if the hashes match.

  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = std::min(index, length - match_length);

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;
  for (wtf_size_t i = 0; i < match_length; ++i) {
    search_hash += search_characters[delta + i];
    match_hash += match_characters[i];
  }

  // keep looping until we match
  while (search_hash != match_hash ||
         !Equal(search_characters + delta, match_characters, match_length)) {
    if (!delta)
      return kNotFound;
    --delta;
    search_hash -= search_characters[delta + match_length];
    search_hash += search_characters[delta];
  }
  return delta;
}

wtf_size_t StringImpl::ReverseFind(const StringView& match_string,
                                   wtf_size_t index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  wtf_size_t match_length = match_string.length();
  wtf_size_t our_length = length();
  if (!match_length)
    return std::min(index, our_length);

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return WTF::ReverseFind(Characters8(), our_length, match_string[0],
                              index);
    return WTF::ReverseFind(Characters16(), our_length, match_string[0], index);
  }

  // Check index & matchLength are in range.
  if (match_length > our_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return ReverseFindInternal(Characters8(), match_string.Characters8(),
                                 index, our_length, match_length);
    return ReverseFindInternal(Characters8(), match_string.Characters16(),
                               index, our_length, match_length);
  }
  if (match_string.Is8Bit())
    return ReverseFindInternal(Characters16(), match_string.Characters8(),
                               index, our_length, match_length);
  return ReverseFindInternal(Characters16(), match_string.Characters16(), index,
                             our_length, match_length);
}

bool StringImpl::StartsWith(UChar character) const {
  return length_ && (*this)[0] == character;
}

bool StringImpl::StartsWith(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit())
      return Equal(Characters8(), prefix.Characters8(), prefix.length());
    return Equal(Characters8(), prefix.Characters16(), prefix.length());
  }
  if (prefix.Is8Bit())
    return Equal(Characters16(), prefix.Characters8(), prefix.length());
  return Equal(Characters16(), prefix.Characters16(), prefix.length());
}

bool StringImpl::StartsWithIgnoringCase(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit()) {
      return DeprecatedEqualIgnoringCase(Characters8(), prefix.Characters8(),
                                         prefix.length());
    }
    return DeprecatedEqualIgnoringCase(Characters8(), prefix.Characters16(),
                                       prefix.length());
  }
  if (prefix.Is8Bit()) {
    return DeprecatedEqualIgnoringCase(Characters16(), prefix.Characters8(),
                                       prefix.length());
  }
  return DeprecatedEqualIgnoringCase(Characters16(), prefix.Characters16(),
                                     prefix.length());
}

bool StringImpl::StartsWithIgnoringASCIICase(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit())
      return EqualIgnoringASCIICase(Characters8(), prefix.Characters8(),
                                    prefix.length());
    return EqualIgnoringASCIICase(Characters8(), prefix.Characters16(),
                                  prefix.length());
  }
  if (prefix.Is8Bit())
    return EqualIgnoringASCIICase(Characters16(), prefix.Characters8(),
                                  prefix.length());
  return EqualIgnoringASCIICase(Characters16(), prefix.Characters16(),
                                prefix.length());
}

bool StringImpl::EndsWith(UChar character) const {
  return length_ && (*this)[length_ - 1] == character;
}

bool StringImpl::EndsWith(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  wtf_size_t start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit())
      return Equal(Characters8() + start_offset, suffix.Characters8(),
                   suffix.length());
    return Equal(Characters8() + start_offset, suffix.Characters16(),
                 suffix.length());
  }
  if (suffix.Is8Bit())
    return Equal(Characters16() + start_offset, suffix.Characters8(),
                 suffix.length());
  return Equal(Characters16() + start_offset, suffix.Characters16(),
               suffix.length());
}

bool StringImpl::EndsWithIgnoringCase(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  wtf_size_t start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit()) {
      return DeprecatedEqualIgnoringCase(Characters8() + start_offset,
                                         suffix.Characters8(), suffix.length());
    }
    return DeprecatedEqualIgnoringCase(Characters8() + start_offset,
                                       suffix.Characters16(), suffix.length());
  }
  if (suffix.Is8Bit()) {
    return DeprecatedEqualIgnoringCase(Characters16() + start_offset,
                                       suffix.Characters8(), suffix.length());
  }
  return DeprecatedEqualIgnoringCase(Characters16() + start_offset,
                                     suffix.Characters16(), suffix.length());
}

bool StringImpl::EndsWithIgnoringASCIICase(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  wtf_size_t start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit())
      return EqualIgnoringASCIICase(Characters8() + start_offset,
                                    suffix.Characters8(), suffix.length());
    return EqualIgnoringASCIICase(Characters8() + start_offset,
                                  suffix.Characters16(), suffix.length());
  }
  if (suffix.Is8Bit())
    return EqualIgnoringASCIICase(Characters16() + start_offset,
                                  suffix.Characters8(), suffix.length());
  return EqualIgnoringASCIICase(Characters16() + start_offset,
                                suffix.Characters16(), suffix.length());
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar old_c, UChar new_c) {
  if (old_c == new_c)
    return this;

  if (Find(old_c) == kNotFound)
    return this;

  wtf_size_t i;
  if (Is8Bit()) {
    if (new_c <= 0xff) {
      LChar* data;
      LChar old_char = static_cast<LChar>(old_c);
      LChar new_char = static_cast<LChar>(new_c);

      scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);

      for (i = 0; i != length_; ++i) {
        LChar ch = Characters8()[i];
        if (ch == old_char)
          ch = new_char;
        data[i] = ch;
      }
      return new_impl;
    }

    // There is the possibility we need to up convert from 8 to 16 bit,
    // create a 16 bit string for the result.
    UChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);

    for (i = 0; i != length_; ++i) {
      UChar ch = Characters8()[i];
      if (ch == old_c)
        ch = new_c;
      data[i] = ch;
    }

    return new_impl;
  }

  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(length_, data);

  for (i = 0; i != length_; ++i) {
    UChar ch = Characters16()[i];
    if (ch == old_c)
      ch = new_c;
    data[i] = ch;
  }
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
  wtf_size_t length_to_insert = string.length();
  if (!length_to_replace && !length_to_insert)
    return this;

  CHECK_LT((length() - length_to_replace),
           (numeric_limits<wtf_size_t>::max() - length_to_insert));

  if (Is8Bit() && (string.IsNull() || string.Is8Bit())) {
    LChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(
        length() - length_to_replace + length_to_insert, data);
    memcpy(data, Characters8(), position * sizeof(LChar));
    if (!string.IsNull())
      memcpy(data + position, string.Characters8(),
             length_to_insert * sizeof(LChar));
    memcpy(data + position + length_to_insert,
           Characters8() + position + length_to_replace,
           (length() - position - length_to_replace) * sizeof(LChar));
    return new_impl;
  }
  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(
      length() - length_to_replace + length_to_insert, data);
  if (Is8Bit())
    for (wtf_size_t i = 0; i < position; ++i)
      data[i] = Characters8()[i];
  else
    memcpy(data, Characters16(), position * sizeof(UChar));
  if (!string.IsNull()) {
    if (string.Is8Bit())
      for (wtf_size_t i = 0; i < length_to_insert; ++i)
        data[i + position] = string.Characters8()[i];
    else
      memcpy(data + position, string.Characters16(),
             length_to_insert * sizeof(UChar));
  }
  if (Is8Bit()) {
    for (wtf_size_t i = 0; i < length() - position - length_to_replace; ++i)
      data[i + position + length_to_insert] =
          Characters8()[i + position + length_to_replace];
  } else {
    memcpy(data + position + length_to_insert,
           Characters16() + position + length_to_replace,
           (length() - position - length_to_replace) * sizeof(UChar));
  }
  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar pattern,
                                              const StringView& replacement) {
  if (replacement.IsNull())
    return this;
  if (replacement.Is8Bit())
    return Replace(pattern, replacement.Characters8(), replacement.length());
  return Replace(pattern, replacement.Characters16(), replacement.length());
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar pattern,
                                              const LChar* replacement,
                                              wtf_size_t rep_str_length) {
  DCHECK(replacement);

  wtf_size_t src_segment_start = 0;
  wtf_size_t match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    ++src_segment_start;
  }

  // If we have 0 matches then we don't have to do any more work.
  if (!match_count)
    return this;

  CHECK(!rep_str_length ||
        match_count <= numeric_limits<wtf_size_t>::max() / rep_str_length);

  wtf_size_t replace_size = match_count * rep_str_length;
  wtf_size_t new_size = length_ - match_count;
  CHECK_LT(new_size, (numeric_limits<wtf_size_t>::max() - replace_size));

  new_size += replace_size;

  // Construct the new data.
  wtf_size_t src_segment_end;
  wtf_size_t src_segment_length;
  src_segment_start = 0;
  wtf_size_t dst_offset = 0;

  if (Is8Bit()) {
    LChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);

    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      memcpy(data + dst_offset, Characters8() + src_segment_start,
             src_segment_length * sizeof(LChar));
      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement, rep_str_length * sizeof(LChar));
      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + 1;
    }

    src_segment_length = length_ - src_segment_start;
    memcpy(data + dst_offset, Characters8() + src_segment_start,
           src_segment_length * sizeof(LChar));

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);

  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));

    dst_offset += src_segment_length;
    for (wtf_size_t i = 0; i < rep_str_length; ++i)
      data[i + dst_offset] = replacement[i];

    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + 1;
  }

  src_segment_length = length_ - src_segment_start;
  memcpy(data + dst_offset, Characters16() + src_segment_start,
         src_segment_length * sizeof(UChar));

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Replace(UChar pattern,
                                              const UChar* replacement,
                                              wtf_size_t rep_str_length) {
  DCHECK(replacement);

  wtf_size_t src_segment_start = 0;
  wtf_size_t match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    ++src_segment_start;
  }

  // If we have 0 matches then we don't have to do any more work.
  if (!match_count)
    return this;

  CHECK(!rep_str_length ||
        match_count <= numeric_limits<wtf_size_t>::max() / rep_str_length);

  wtf_size_t replace_size = match_count * rep_str_length;
  wtf_size_t new_size = length_ - match_count;
  CHECK_LT(new_size, (numeric_limits<wtf_size_t>::max() - replace_size));

  new_size += replace_size;

  // Construct the new data.
  wtf_size_t src_segment_end;
  wtf_size_t src_segment_length;
  src_segment_start = 0;
  wtf_size_t dst_offset = 0;

  if (Is8Bit()) {
    UChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);

    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      for (wtf_size_t i = 0; i < src_segment_length; ++i)
        data[i + dst_offset] = Characters8()[i + src_segment_start];

      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement, rep_str_length * sizeof(UChar));

      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + 1;
    }

    src_segment_length = length_ - src_segment_start;
    for (wtf_size_t i = 0; i < src_segment_length; ++i)
      data[i + dst_offset] = Characters8()[i + src_segment_start];

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);

  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));

    dst_offset += src_segment_length;
    memcpy(data + dst_offset, replacement, rep_str_length * sizeof(UChar));

    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + 1;
  }

  src_segment_length = length_ - src_segment_start;
  memcpy(data + dst_offset, Characters16() + src_segment_start,
         src_segment_length * sizeof(UChar));

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::Replace(const StringView& pattern,
                                              const StringView& replacement) {
  if (pattern.IsNull() || replacement.IsNull())
    return this;

  wtf_size_t pattern_length = pattern.length();
  if (!pattern_length)
    return this;

  wtf_size_t rep_str_length = replacement.length();
  wtf_size_t src_segment_start = 0;
  wtf_size_t match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    src_segment_start += pattern_length;
  }

  // If we have 0 matches, we don't have to do any more work
  if (!match_count)
    return this;

  wtf_size_t new_size = length_ - match_count * pattern_length;
  CHECK(!rep_str_length ||
        match_count <= numeric_limits<wtf_size_t>::max() / rep_str_length);

  CHECK_LE(new_size,
           (numeric_limits<wtf_size_t>::max() - match_count * rep_str_length));

  new_size += match_count * rep_str_length;

  // Construct the new data
  wtf_size_t src_segment_end;
  wtf_size_t src_segment_length;
  src_segment_start = 0;
  wtf_size_t dst_offset = 0;
  bool src_is_8bit = Is8Bit();
  bool replacement_is_8bit = replacement.Is8Bit();

  // There are 4 cases:
  // 1. This and replacement are both 8 bit.
  // 2. This and replacement are both 16 bit.
  // 3. This is 8 bit and replacement is 16 bit.
  // 4. This is 16 bit and replacement is 8 bit.
  if (src_is_8bit && replacement_is_8bit) {
    // Case 1
    LChar* data;
    scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      memcpy(data + dst_offset, Characters8() + src_segment_start,
             src_segment_length * sizeof(LChar));
      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement.Characters8(),
             rep_str_length * sizeof(LChar));
      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + pattern_length;
    }

    src_segment_length = length_ - src_segment_start;
    memcpy(data + dst_offset, Characters8() + src_segment_start,
           src_segment_length * sizeof(LChar));

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  scoped_refptr<StringImpl> new_impl = CreateUninitialized(new_size, data);
  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    if (src_is_8bit) {
      // Case 3.
      for (wtf_size_t i = 0; i < src_segment_length; ++i)
        data[i + dst_offset] = Characters8()[i + src_segment_start];
    } else {
      // Case 2 & 4.
      memcpy(data + dst_offset, Characters16() + src_segment_start,
             src_segment_length * sizeof(UChar));
    }
    dst_offset += src_segment_length;
    if (replacement_is_8bit) {
      // Cases 2 & 3.
      for (wtf_size_t i = 0; i < rep_str_length; ++i)
        data[i + dst_offset] = replacement.Characters8()[i];
    } else {
      // Case 4
      memcpy(data + dst_offset, replacement.Characters16(),
             rep_str_length * sizeof(UChar));
    }
    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + pattern_length;
  }

  src_segment_length = length_ - src_segment_start;
  if (src_is_8bit) {
    // Case 3.
    for (wtf_size_t i = 0; i < src_segment_length; ++i)
      data[i + dst_offset] = Characters8()[i + src_segment_start];
  } else {
    // Cases 2 & 4.
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));
  }

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

scoped_refptr<StringImpl> StringImpl::UpconvertedString() {
  if (Is8Bit())
    return String::Make16BitFrom8BitSource(Characters8(), length_)
        .ReleaseImpl();
  return this;
}

static inline bool StringImplContentEqual(const StringImpl* a,
                                          const StringImpl* b) {
  wtf_size_t a_length = a->length();
  wtf_size_t b_length = b->length();
  if (a_length != b_length)
    return false;

  if (a->Is8Bit()) {
    if (b->Is8Bit())
      return Equal(a->Characters8(), b->Characters8(), a_length);

    return Equal(a->Characters8(), b->Characters16(), a_length);
  }

  if (b->Is8Bit())
    return Equal(a->Characters16(), b->Characters8(), a_length);

  return Equal(a->Characters16(), b->Characters16(), a_length);
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
inline bool EqualInternal(const StringImpl* a,
                          const CharType* b,
                          wtf_size_t length) {
  if (!a)
    return !b;
  if (!b)
    return false;

  if (a->length() != length)
    return false;
  if (a->Is8Bit())
    return Equal(a->Characters8(), b, length);
  return Equal(a->Characters16(), b, length);
}

bool Equal(const StringImpl* a, const LChar* b, wtf_size_t length) {
  return EqualInternal(a, b, length);
}

bool Equal(const StringImpl* a, const UChar* b, wtf_size_t length) {
  return EqualInternal(a, b, length);
}

bool Equal(const StringImpl* a, const LChar* b) {
  if (!a)
    return !b;
  if (!b)
    return !a;

  wtf_size_t length = a->length();

  if (a->Is8Bit()) {
    const LChar* a_ptr = a->Characters8();
    for (wtf_size_t i = 0; i != length; ++i) {
      LChar bc = b[i];
      LChar ac = a_ptr[i];
      if (!bc)
        return false;
      if (ac != bc)
        return false;
    }

    return !b[length];
  }

  const UChar* a_ptr = a->Characters16();
  for (wtf_size_t i = 0; i != length; ++i) {
    LChar bc = b[i];
    if (!bc)
      return false;
    if (a_ptr[i] != bc)
      return false;
  }

  return !b[length];
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

template <typename CharacterType1, typename CharacterType2>
int CodeUnitCompareIgnoringASCIICase(wtf_size_t l1,
                                     wtf_size_t l2,
                                     const CharacterType1* c1,
                                     const CharacterType2* c2) {
  const wtf_size_t lmin = l1 < l2 ? l1 : l2;
  wtf_size_t pos = 0;
  while (pos < lmin && ToASCIILower(*c1) == ToASCIILower(*c2)) {
    ++c1;
    ++c2;
    ++pos;
  }

  if (pos < lmin)
    return (ToASCIILower(c1[0]) > ToASCIILower(c2[0])) ? 1 : -1;

  if (l1 == l2)
    return 0;

  return (l1 > l2) ? 1 : -1;
}

int CodeUnitCompareIgnoringASCIICase(const StringImpl* string1,
                                     const LChar* string2) {
  wtf_size_t length1 = string1 ? string1->length() : 0;
  wtf_size_t length2 = SafeCast<wtf_size_t>(
      string2 ? strlen(reinterpret_cast<const char*>(string2)) : 0);

  if (!string1)
    return length2 > 0 ? -1 : 0;

  if (!string2)
    return length1 > 0 ? 1 : 0;

  if (string1->Is8Bit()) {
    return CodeUnitCompareIgnoringASCIICase(length1, length2,
                                            string1->Characters8(), string2);
  }
  return CodeUnitCompareIgnoringASCIICase(length1, length2,
                                          string1->Characters16(), string2);
}

}  // namespace WTF
