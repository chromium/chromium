// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_IMAGE_INFO_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_IMAGE_INFO_HASH_H_

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace WTF {

template <>
struct DefaultHash<SkImageInfo> {
  STATIC_ONLY(DefaultHash);
  static unsigned GetHash(const SkImageInfo& key) {
    unsigned result = HashInts(key.width(), key.height());
    result = HashInts(result, key.colorType());
    result = HashInts(result, key.alphaType());
    if (auto* cs = key.colorSpace())
      result = HashInts(result, static_cast<uint32_t>(cs->hash()));
    return result;
  }
  static bool Equal(const SkImageInfo& a, const SkImageInfo& b) {
    return a == b;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

template <>
struct HashTraits<SkImageInfo> : GenericHashTraits<SkImageInfo> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = true;
  static SkImageInfo EmptyValue() {
    return SkImageInfo::Make(0, 0, kUnknown_SkColorType, kUnknown_SkAlphaType,
                             nullptr);
  }
  static void ConstructDeletedValue(SkImageInfo& slot, bool) {
    slot = SkImageInfo::Make(-1, -1, kUnknown_SkColorType, kUnknown_SkAlphaType,
                             nullptr);
  }
  static bool IsDeletedValue(const SkImageInfo& value) {
    return value.width() == -1 && value.height() == -1 &&
           value.colorType() == kUnknown_SkColorType &&
           value.alphaType() == kUnknown_SkAlphaType &&
           value.colorSpace() == nullptr;
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SK_IMAGE_INFO_HASH_H_
