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

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

// The GetHash() functions on StringHash do not support null strings. find(),
// Contains(), and insert() on HashMap<String,..., StringHash> cause a
// null-pointer dereference when passed null strings.
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

// This hash can be used in cases where the key is a hash of a string, but we
// don't want to store the string. It's not really specific to string hashing,
// but all our current uses of it are for strings.
struct AlreadyHashed : IntHash<unsigned> {
  STATIC_ONLY(AlreadyHashed);
  static unsigned GetHash(unsigned key) { return key; }
};

// StringHash is the default hash for String, StringImpl* and
// scopoed_refptr<StringImpl>.
template <>
struct DefaultHash<String> : StringHash {};
template <>
struct DefaultHash<StringImpl*> : StringHash {};
template <>
struct DefaultHash<scoped_refptr<StringImpl>> : StringHash {};

}  // namespace WTF

namespace std {
template <>
struct hash<WTF::String> {
  size_t operator()(const WTF::String& string) const {
    return WTF::StringHash::GetHash(string);
  }
};
}  // namespace std

using WTF::AlreadyHashed;
using WTF::StringHash;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASH_H_
