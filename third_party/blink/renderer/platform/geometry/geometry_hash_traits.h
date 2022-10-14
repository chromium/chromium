// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_

#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/size_f.h"

namespace WTF {

template <>
struct DefaultHash<gfx::SizeF> {
  STATIC_ONLY(DefaultHash);
  static unsigned GetHash(const gfx::SizeF& key) {
    return HashInts(DefaultHash<float>::GetHash(key.width()),
                    DefaultHash<float>::GetHash(key.height()));
  }
  static bool Equal(const gfx::SizeF& a, const gfx::SizeF& b) {
    return DefaultHash<float>::Equal(a.width(), b.width()) &&
           DefaultHash<float>::Equal(a.height(), b.height());
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
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

template <>
struct DefaultHash<SkIRect> {
  STATIC_ONLY(DefaultHash);
  static unsigned GetHash(const SkIRect& key) {
    return HashInts(HashInts(key.x(), key.y()),
                    HashInts(key.right(), key.bottom()));
  }
  static bool Equal(const SkIRect& a, const SkIRect& b) { return a == b; }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

template <>
struct HashTraits<SkIRect> : GenericHashTraits<SkIRect> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = false;
  static SkIRect EmptyValue() { return SkIRect::MakeWH(-1, 0); }
  static void ConstructDeletedValue(SkIRect& slot, bool) {
    slot = DeletedValue();
  }
  static bool IsDeletedValue(const SkIRect& value) {
    return value == DeletedValue();
  }

 private:
  static constexpr SkIRect DeletedValue() { return SkIRect::MakeWH(0, -1); }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_HASH_TRAITS_H_
