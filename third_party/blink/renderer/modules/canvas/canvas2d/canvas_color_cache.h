// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_COLOR_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_COLOR_CACHE_H_

#include <memory>

#include "base/containers/lru_cache.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

// Value stored by the cache.
struct MODULES_EXPORT CachedColor {
  CachedColor(const Color& color, ColorParseResult parse_result)
      : color(color), parse_result(parse_result) {}

  Color color;
  ColorParseResult parse_result;
};

// CanvasColorCache is an lru cache of colors used by canvas. The cache is
// keyed by String.
class MODULES_EXPORT CanvasColorCache final {
 public:
  // Creates a new instance. Returns null if cache is not enabled.
  static std::unique_ptr<CanvasColorCache> Create();

  ~CanvasColorCache() = default;

  // Returns the cached color, or null if not present.
  const CachedColor* GetCachedColor(const AtomicString& string);

  // Adds a color to the cache.
  void SetCachedColor(const AtomicString& string,
                      const Color& color,
                      ColorParseResult parse_result);

  void Clear() { cache_.Clear(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(CanvasColorCacheTest, Histograms);

  explicit CanvasColorCache(int cache_size) : cache_(cache_size) {}

  void LogCacheEffectiveness();

  // Counters for how measuring effectiveness of the cache.
  uint32_t cache_hit_count_ = 0;
  uint32_t cache_miss_count_ = 0;

  base::HashingLRUCache<String, CachedColor> cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_COLOR_CACHE_H_
