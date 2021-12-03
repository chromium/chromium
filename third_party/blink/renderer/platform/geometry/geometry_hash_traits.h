// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_

#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "ui/gfx/geometry/size_f.h"

namespace WTF {

template <>
struct DefaultHash<gfx::SizeF> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    typedef typename IntTypes<sizeof(float)>::UnsignedType Bits;
    static unsigned GetHash(const gfx::SizeF& key) {
      return HashInts(DefaultHash<float>::Hash::GetHash(key.width()),
                      DefaultHash<float>::Hash::GetHash(key.height()));
    }
    static bool Equal(const gfx::SizeF& a, const gfx::SizeF& b) {
      return DefaultHash<float>::Hash::Equal(a.width(), b.width()) &&
             DefaultHash<float>::Hash::Equal(a.height(), b.height());
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<gfx::SizeF> : GenericHashTraits<gfx::SizeF> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = false;
  static gfx::SizeF EmptyValue() {
    return gfx::SizeF(std::numeric_limits<float>::infinity(), 0);
  }
  static void ConstructDeletedValue(gfx::SizeF& slot, bool) {
    slot = DeletedValue();
  }
  static bool IsDeletedValue(const gfx::SizeF& value) {
    return value == DeletedValue();
  }

 private:
  static constexpr gfx::SizeF DeletedValue() {
    return gfx::SizeF(0, std::numeric_limits<float>::infinity());
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_
