// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CACHE_H_

#include <unordered_map>

#include "base/hash/hash.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {

// DarkModeImageCache - Implements dark mode filter cache for different |src|
// rects from the image.
class PLATFORM_EXPORT DarkModeImageCache {
 public:
  DarkModeImageCache() = default;
  ~DarkModeImageCache() = default;

  bool Exists(const SkIRect& src) {
    return cache_.find(DarkModeKey(src)) != cache_.end();
  }

  sk_sp<SkColorFilter> Get(const SkIRect& src) {
    auto result = cache_.find(DarkModeKey(src));
    return (result != cache_.end()) ? result->second : nullptr;
  }

  void Add(const SkIRect& src, sk_sp<SkColorFilter> dark_mode_color_filter) {
    DCHECK(!Exists(src));

    cache_.emplace(DarkModeKey(src), std::move(dark_mode_color_filter));
  }

  size_t Size() { return cache_.size(); }

  void Clear() { cache_.clear(); }

 private:
  struct DarkModeKeyHash;
  struct DarkModeKey {
    explicit DarkModeKey(SkIRect src) : src_(src) {}

    bool operator==(const DarkModeKey& other) const {
      return src_ == other.src_;
    }

   private:
    SkIRect src_;

    friend struct DarkModeImageCache::DarkModeKeyHash;
  };

  struct DarkModeKeyHash {
    size_t operator()(const DarkModeKey& key) const {
      return base::HashInts(
          base::HashInts(base::HashInts(key.src_.x(), key.src_.y()),
                         key.src_.width()),
          key.src_.height());
    }
  };

  std::unordered_map<DarkModeKey, sk_sp<SkColorFilter>, DarkModeKeyHash> cache_;

  DISALLOW_COPY_AND_ASSIGN(DarkModeImageCache);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CACHE_H_
