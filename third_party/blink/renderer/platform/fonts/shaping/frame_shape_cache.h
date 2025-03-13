// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FRAME_SHAPE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FRAME_SHAPE_CACHE_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// FrameShapeCache manages a cache for PlainTextNode, and a cache for
// ShapeResult and ink bounds. A PlainTextPainter instance owns multiple
// FrameShapeCache instances, and each FrameShapeCache instance is associated
// to a specific `Font`.
//
// The caches are aware of frames distinguished by DidSwitchFrame() calls. This
// allows the caches to purge old entries when the frame is switched.
class FrameShapeCache : public GarbageCollected<FrameShapeCache> {
 public:
  FrameShapeCache();
  void Trace(Visitor* visitor) const;

  FrameShapeCache(const FrameShapeCache&) = delete;
  FrameShapeCache& operator=(const FrameShapeCache&) = delete;

  // This function should be called between the end of an animation frame and
  // the beginning of the next animation frame.
  void DidSwitchFrame();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FRAME_SHAPE_CACHE_H_
