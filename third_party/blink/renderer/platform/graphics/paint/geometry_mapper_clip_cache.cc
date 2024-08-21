// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// All clip caches invalidate themselves by tracking a local cache generation,
// and invalidating their cache if their cache generation disagrees with
// s_global_generation_.
unsigned GeometryMapperClipCache::s_global_generation_ = 1;

void GeometryMapperClipCache::ClipAndTransform::Trace(Visitor* visitor) const {
  visitor->Trace(ancestor_clip);
  visitor->Trace(ancestor_transform);
}

void GeometryMapperClipCache::Trace(Visitor* visitor) const {
  visitor->Trace(clip_cache_);
  visitor->Trace(nearest_pixel_moving_filter_clip_);
}

void GeometryMapperClipCache::ClearCache() {
  s_global_generation_++;
}

bool GeometryMapperClipCache::IsValid() const {
  return cache_generation_ == s_global_generation_;
}

void GeometryMapperClipCache::Update(const ClipPaintPropertyNode& node) {
  DCHECK_NE(cache_generation_, s_global_generation_);
  cache_generation_ = s_global_generation_;

  clip_cache_.clear();

  if (node.PixelMovingFilter()) {
    nearest_pixel_moving_filter_clip_ = &node;
  } else if (const auto* parent = node.UnaliasedParent()) {
    nearest_pixel_moving_filter_clip_ =
        parent->GetClipCache().nearest_pixel_moving_filter_clip_;
  } else {
    nearest_pixel_moving_filter_clip_ = nullptr;
  }
}

const GeometryMapperClipCache::ClipCacheEntry*
GeometryMapperClipCache::GetCachedClip(
    const ClipAndTransform& clip_and_transform) {
  DCHECK(IsValid());
  for (const auto& entry : clip_cache_) {
    if (entry.clip_and_transform == clip_and_transform) {
      return &entry;
    }
  }
  return nullptr;
}

void GeometryMapperClipCache::SetCachedClip(const ClipCacheEntry& entry) {
  DCHECK(IsValid());
  // There should be no existing entry.
  DCHECK(!GetCachedClip(entry.clip_and_transform));
  clip_cache_.push_back(entry);
}

}  // namespace blink
