// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_color_cache.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"

namespace blink {
namespace {

BASE_FEATURE(kCanvasColorCache,
             "CanvasColorCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kCanvasColorCacheSize{&kCanvasColorCache,
                                                    "cache-size", 8};

}  // namespace

// static
std::unique_ptr<CanvasColorCache> CanvasColorCache::Create() {
  if (!base::FeatureList::IsEnabled(kCanvasColorCache)) {
    return nullptr;
  }

  return base::WrapUnique(new CanvasColorCache(kCanvasColorCacheSize.Get()));
}

const CachedColor* CanvasColorCache::GetCachedColor(
    const AtomicString& string) {
  DCHECK(!string.IsNull());
  if (string.empty()) {
    return nullptr;
  }

  auto it = cache_.Get(string);
  if (it == cache_.end()) {
    ++cache_miss_count_;
    LogCacheEffectiveness();
    return nullptr;
  }
  ++cache_hit_count_;
  LogCacheEffectiveness();
  return &it->second;
}

void CanvasColorCache::SetCachedColor(const AtomicString& string,
                                      const Color& color,
                                      ColorParseResult parse_result) {
  DCHECK(!string.IsNull());
  cache_.Put(string, CachedColor(color, parse_result));
}

void CanvasColorCache::LogCacheEffectiveness() {
  // This function is called a lot, so it only records every so often.
  if ((cache_hit_count_ + cache_miss_count_) % 1000 == 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Blink.Canvas.CanvasColorCache.Effectiveness",
        cache_hit_count_ * 100 / (cache_hit_count_ + cache_miss_count_));
    cache_hit_count_ = cache_miss_count_ = 0;
  }
}

}  // namespace blink
