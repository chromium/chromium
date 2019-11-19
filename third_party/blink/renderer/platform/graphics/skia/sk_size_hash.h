/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_SIZE_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_SIZE_HASH_H_

#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSize.h"

namespace WTF {

template <>
struct DefaultHash<SkSize> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const SkSize& key) {
      return HashInts(key.width(), key.height());
    }
    static bool Equal(const SkSize& a, const SkSize& b) { return a == b; }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<SkSize> : GenericHashTraits<SkSize> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = true;
  static SkSize EmptyValue() { return SkSize::Make(0, 0); }
  static void ConstructDeletedValue(SkSize& slot, bool) {
    slot = SkSize::Make(-1, -1);
  }
  static bool IsDeletedValue(const SkSize& value) {
    return value.width() == -1 && value.height() == -1;
  }
};

template <>
struct DefaultHash<SkISize> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const SkISize& key) {
      return HashInts(key.width(), key.height());
    }
    static bool Equal(const SkISize& a, const SkISize& b) { return a == b; }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<SkISize> : GenericHashTraits<SkISize> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = true;
  static SkISize EmptyValue() { return SkISize::Make(0, 0); }
  static void ConstructDeletedValue(SkISize& slot, bool) {
    slot = SkISize::Make(-1, -1);
  }
  static bool IsDeletedValue(const SkISize& value) {
    return value.width() == -1 && value.height() == -1;
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_SIZE_HASH_H_
