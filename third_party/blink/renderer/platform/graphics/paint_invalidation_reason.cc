// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"

#include <ostream>

#include "base/notreached.h"

namespace blink {

static_assert(static_cast<uint8_t>(PaintInvalidationReason::kMax) < (1 << 6),
              "PaintInvalidationReason must fit in 6 bits");

const char* PaintInvalidationReasonToString(PaintInvalidationReason reason) {
  switch (reason) {
    case PaintInvalidationReason::kNone:
      return "none";
    case PaintInvalidationReason::kIncremental:
      return "incremental";
    case PaintInvalidationReason::kHitTest:
      return "hit testing change";
    case PaintInvalidationReason::kStyle:
      return "style change";
    case PaintInvalidationReason::kOutline:
      return "outline";
    case PaintInvalidationReason::kImage:
      return "image";
    case PaintInvalidationReason::kBackground:
      return "background";
    case PaintInvalidationReason::kBackplate:
      return "backplate";
    case PaintInvalidationReason::kLayout:
      return "geometry";
    case PaintInvalidationReason::kAppeared:
      return "appeared";
    case PaintInvalidationReason::kDisappeared:
      return "disappeared";
    case PaintInvalidationReason::kScrollControl:
      return "scroll control";
    case PaintInvalidationReason::kSelection:
      return "selection";
    case PaintInvalidationReason::kSubtree:
      return "subtree";
    case PaintInvalidationReason::kSVGResource:
      return "SVG resource change";
    case PaintInvalidationReason::kCaret:
      return "caret";
    case PaintInvalidationReason::kDocumentMarker:
      return "DocumentMarker change";
    case PaintInvalidationReason::kUncacheable:
      return "uncacheable";
    case PaintInvalidationReason::kJustCreated:
      return "just created";
    case PaintInvalidationReason::kReordered:
      return "reordered";
    case PaintInvalidationReason::kChunkAppeared:
      return "chunk appeared";
    case PaintInvalidationReason::kChunkDisappeared:
      return "chunk disappeared";
    case PaintInvalidationReason::kChunkUncacheable:
      return "chunk uncacheable";
    case PaintInvalidationReason::kChunkReordered:
      return "chunk reordered";
    case PaintInvalidationReason::kPaintProperty:
      return "paint property change";
    case PaintInvalidationReason::kFullLayer:
      return "full layer";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::ostream& operator<<(std::ostream& out, PaintInvalidationReason reason) {
  return out << PaintInvalidationReasonToString(reason);
}

}  // namespace blink
