// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_ICU_LOCALE_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_ICU_LOCALE_HASH_TRAITS_H_

#include <cstring>

#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace blink {

// HashTraits for icu::Locale to enable use as a key in WTF::HashMap.
// Uses getName() for hashing which returns ICU's canonical form ("en_US").
// Root locale ("") is used as empty value since it's not a valid map key.
template <>
struct HashTraits<icu::Locale> : GenericHashTraits<icu::Locale> {
  static unsigned GetHash(const icu::Locale& key) {
    const char* name = key.getName();
    return StringHasher::ComputeHashAndMaskTop8Bits(name, std::strlen(name));
  }

  // We use Root locale ("") as our empty value, as it is an invalid key.
  static icu::Locale EmptyValue() { return icu::Locale::getRoot(); }

  // Private-use subtag "x-"
  static void ConstructDeletedValue(icu::Locale& slot) {
    slot = icu::Locale("x-deleted-value");
  }

  static bool IsDeletedValue(const icu::Locale& value) {
    return value == icu::Locale("x-deleted-value");
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_ICU_LOCALE_HASH_TRAITS_H_
