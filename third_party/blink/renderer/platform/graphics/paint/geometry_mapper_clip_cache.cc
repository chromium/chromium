// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"

#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"

namespace blink {

// All clip caches invalidate themselves by tracking a local cache generation,
// and invalidating their cache if their cache generation disagrees with
// s_clipCacheGeneration.
static unsigned g_clip_cache_generation = 0;

GeometryMapperClipCache::GeometryMapperClipCache()
    : cache_generation_(g_clip_cache_generation) {}

void GeometryMapperClipCache::ClearCache() {
  g_clip_cache_generation++;
}

bool GeometryMapperClipCache::IsValid() const {
  return cache_generation_ == g_clip_cache_generation;
}

void GeometryMapperClipCache::InvalidateCacheIfNeeded() {
  if (cache_generation_ != g_clip_cache_generation) {
    clip_cache_.clear();
    cache_generation_ = g_clip_cache_generation;
  }
}

const GeometryMapperClipCache::ClipCacheEntry*
GeometryMapperClipCache::GetCachedClip(
    const ClipAndTransform& clip_and_transform) {
  InvalidateCacheIfNeeded();
  for (const auto& entry : clip_cache_) {
    if (entry.clip_and_transform == clip_and_transform) {
      return &entry;
    }
  }
  return nullptr;
}

void GeometryMapperClipCache::SetCachedClip(const ClipCacheEntry& entry) {
  InvalidateCacheIfNeeded();
  // There should be no existing entry.
  DCHECK(!GetCachedClip(entry.clip_and_transform));
  clip_cache_.push_back(entry);
}

}  // namespace blink
