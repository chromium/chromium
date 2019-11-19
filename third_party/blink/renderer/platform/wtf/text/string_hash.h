/*
 * Copyright (C) 2006, 2007, 2008, 2012, 2013 Apple Inc. All rights reserved
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASH_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace WTF {

inline bool HashTraits<String>::IsEmptyValue(const String& value) {
  return value.IsNull();
}

inline bool HashTraits<String>::IsDeletedValue(const String& value) {
  return HashTraits<scoped_refptr<StringImpl>>::IsDeletedValue(value.impl_);
}

inline void HashTraits<String>::ConstructDeletedValue(String& slot,
                                                      bool zero_value) {
  HashTraits<scoped_refptr<StringImpl>>::ConstructDeletedValue(slot.impl_,
                                                               zero_value);
}

// The hash() functions on StringHash and CaseFoldingHash do not support null
// strings. get(), contains(), and add() on HashMap<String,..., StringHash>
// cause a null-pointer dereference when passed null strings.

// FIXME: We should really figure out a way to put the computeHash function
// that's currently a member function of StringImpl into this file so we can be
// a little closer to having all the nearly-identical hash functions in one
// place.

struct StringHash {
  STATIC_ONLY(StringHash);
  static unsigned GetHash(StringImpl* key) { return key->GetHash(); }
  static inline bool Equal(const StringImpl* a, const StringImpl* b) {
    return EqualNonNull(a, b);
  }

  static unsigned GetHash(const scoped_refptr<StringImpl>& key) {
    return key->GetHash();
  }
  static bool Equal(const scoped_refptr<StringImpl>& a,
                    const scoped_refptr<StringImpl>& b) {
    return Equal(a.get(), b.get());
  }

  static unsigned GetHash(const String& key) { return key.Impl()->GetHash(); }
  static bool Equal(const String& a, const String& b) {
    return Equal(a.Impl(), b.Impl());
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

class CaseFoldingHash {
  STATIC_ONLY(CaseFoldingHash);

 public:
  static unsigned GetHash(const UChar* data, unsigned length) {
    return StringHasher::ComputeHashAndMaskTop8Bits<UChar, FoldCase<UChar>>(
        data, length);
  }

  static unsigned GetHash(StringImpl* str) {
    if (str->Is8Bit())
      return GetHash(str->Characters8(), str->length());
    return GetHash(str->Characters16(), str->length());
  }

  static unsigned GetHash(const LChar* data, unsigned length) {
    return StringHasher::ComputeHashAndMaskTop8Bits<LChar, FoldCase<LChar>>(
        data, length);
  }

  static inline unsigned GetHash(const char* data, unsigned length) {
    return CaseFoldingHash::GetHash(reinterpret_cast<const LChar*>(data),
                                    length);
  }

  static inline bool Equal(const StringImpl* a, const StringImpl* b) {
    DCHECK(a);
    DCHECK(b);
    // Save one branch inside each StringView by derefing the StringImpl,
    // and another branch inside the compare function by skipping the null
    // checks.
    return DeprecatedEqualIgnoringCaseAndNullity(*a, *b);
  }

  static unsigned GetHash(const scoped_refptr<StringImpl>& key) {
    return GetHash(key.get());
  }

  static bool Equal(const scoped_refptr<StringImpl>& a,
                    const scoped_refptr<StringImpl>& b) {
    return Equal(a.get(), b.get());
  }

  static unsigned GetHash(const String& key) { return GetHash(key.Impl()); }
  static unsigned GetHash(const AtomicString& key) {
    return GetHash(key.Impl());
  }
  static bool Equal(const String& a, const String& b) {
    return Equal(a.Impl(), b.Impl());
  }
  static bool Equal(const AtomicString& a, const AtomicString& b) {
    return (a == b) || Equal(a.Impl(), b.Impl());
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;

 private:
  // Private so no one uses this in the belief that it will return the
  // correctly-folded code point in all cases (see comment below).
  template <typename T>
  static inline UChar FoldCase(T ch) {
    if (std::is_same<T, LChar>::value)
      return StringImpl::kLatin1CaseFoldTable[ch];
    // It's possible for WTF::unicode::foldCase() to return a 32-bit value
    // that's not representable as a UChar.  However, since this is rare and
    // deterministic, and the result of this is merely used for hashing, go
    // ahead and clamp the value.
    return static_cast<UChar>(WTF::unicode::FoldCase(ch));
  }
};

// This hash can be used in cases where the key is a hash of a string, but we
// don't want to store the string. It's not really specific to string hashing,
// but all our current uses of it are for strings.
struct AlreadyHashed : IntHash<unsigned> {
  STATIC_ONLY(AlreadyHashed);
  static unsigned GetHash(unsigned key) { return key; }

  // To use a hash value as a key for a hash table, we need to eliminate the
  // "deleted" value, which is negative one. That could be done by changing
  // the string hash function to never generate negative one, but this works
  // and is still relatively efficient.
  static unsigned AvoidDeletedValue(unsigned hash) {
    DCHECK(hash);
    unsigned new_hash = hash | (!(hash + 1) << 31);
    DCHECK(new_hash);
    DCHECK_NE(new_hash, 0xFFFFFFFF);
    return new_hash;
  }
};

}  // namespace WTF

using WTF::AlreadyHashed;
using WTF::CaseFoldingHash;
using WTF::StringHash;

#endif
