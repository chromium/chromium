// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_INVALIDATION_REASON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_INVALIDATION_REASON_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

enum class PaintInvalidationReason : uint8_t {
  kNone,
  kIncremental,
  kRectangle,
  kSelection,
  // Hit test changes do not require raster invalidation.
  kHitTest,
  // The following reasons will all cause full paint invalidation.
  // Any unspecified reason of full invalidation.
  kFull,
  kStyle,
  // Layout or visual geometry change.
  kBackplate,
  kGeometry,
  kCompositing,
  kAppeared,
  kDisappeared,
  kScroll,
  // Scroll bars, scroll corner, etc.
  kScrollControl,
  kOutline,
  // The object is invalidated as a part of a subtree full invalidation (forced
  // by LayoutObject::SetSubtreeShouldDoFullPaintInvalidation()).
  kSubtree,
  kSVGResource,
  kBackground,
  kCaret,
  kDocumentMarker,
  kImage,
  kUncacheable,
  // The initial PaintInvalidationReason of a DisplayItemClient.
  kJustCreated,
  kReordered,
  kChunkAppeared,
  kChunkDisappeared,
  kChunkUncacheable,
  kChunkReordered,
  kPaintProperty,
  // For tracking of direct raster invalidation of full composited layers. The
  // invalidation may be implicit, e.g. when a layer is created.
  kFullLayer,
  kForTesting,
  kMax = kForTesting,
};

PLATFORM_EXPORT const char* PaintInvalidationReasonToString(
    PaintInvalidationReason);

inline bool IsFullPaintInvalidationReason(PaintInvalidationReason reason) {
  return reason >= PaintInvalidationReason::kFull;
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         PaintInvalidationReason);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_INVALIDATION_REASON_H_
