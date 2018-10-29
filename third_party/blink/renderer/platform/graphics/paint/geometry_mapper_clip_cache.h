// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_CLIP_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_CLIP_CACHE_H_

#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ClipPaintPropertyNode;
class FloatClipRect;
class TransformPaintPropertyNode;

// A GeometryMapperClipCache hangs off a ClipPaintPropertyNode. It stores
// cached "clip visual rects" (See GeometryMapper.h) from that node in
// ancestor spaces.
class PLATFORM_EXPORT GeometryMapperClipCache {
  USING_FAST_MALLOC(GeometryMapperClipCache);

 public:
  GeometryMapperClipCache();

  struct ClipAndTransform {
    const ClipPaintPropertyNode* ancestor_clip;
    const TransformPaintPropertyNode* ancestor_transform;
    OverlayScrollbarClipBehavior clip_behavior;
    bool operator==(const ClipAndTransform& other) const {
      return ancestor_clip == other.ancestor_clip &&
             ancestor_transform == other.ancestor_transform &&
             clip_behavior == other.clip_behavior;
    }
    ClipAndTransform(const ClipPaintPropertyNode* ancestor_clip_arg,
                     const TransformPaintPropertyNode* ancestor_transform_arg,
                     OverlayScrollbarClipBehavior clip_behavior_arg)
        : ancestor_clip(ancestor_clip_arg),
          ancestor_transform(ancestor_transform_arg),
          clip_behavior(clip_behavior_arg) {}
  };

  // Returns the clip visual rect  of the owning
  // clip of |this| in the space of |ancestors|, if there is one cached.
  // Otherwise returns null.
  const FloatClipRect* GetCachedClip(const ClipAndTransform& ancestors);

  // Stores the "clip visual rect" of |this in the space of |ancestors|,
  // into a local cache.
  void SetCachedClip(const ClipAndTransform&, const FloatClipRect&);

  static void ClearCache();
  bool IsValid() const;

 private:
  struct ClipCacheEntry {
    const ClipAndTransform clip_and_transform;
    const FloatClipRect clip_rect;
    ClipCacheEntry(const ClipAndTransform& clip_and_transform_arg,
                   const FloatClipRect& clip_rect_arg)
        : clip_and_transform(clip_and_transform_arg),
          clip_rect(clip_rect_arg) {}
  };

  void InvalidateCacheIfNeeded();

  Vector<ClipCacheEntry> clip_cache_;
  unsigned cache_generation_;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapperClipCache);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_CLIP_CACHE_H_
