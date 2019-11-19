// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

const char* PaintInvalidationReasonToString(PaintInvalidationReason reason) {
  switch (reason) {
    case PaintInvalidationReason::kNone:
      return "none";
    case PaintInvalidationReason::kIncremental:
      return "incremental";
    case PaintInvalidationReason::kRectangle:
      return "invalidate paint rectangle";
    case PaintInvalidationReason::kHitTest:
      return "hit testing change";
    case PaintInvalidationReason::kFull:
      return "full";
    case PaintInvalidationReason::kStyle:
      return "style change";
    case PaintInvalidationReason::kBackplate:
      return "backplate";
    case PaintInvalidationReason::kGeometry:
      return "geometry";
    case PaintInvalidationReason::kCompositing:
      return "compositing update";
    case PaintInvalidationReason::kBackground:
      return "background";
    case PaintInvalidationReason::kAppeared:
      return "appeared";
    case PaintInvalidationReason::kDisappeared:
      return "disappeared";
    case PaintInvalidationReason::kScroll:
      return "scroll";
    case PaintInvalidationReason::kScrollControl:
      return "scroll control";
    case PaintInvalidationReason::kSelection:
      return "selection";
    case PaintInvalidationReason::kOutline:
      return "outline";
    case PaintInvalidationReason::kSubtree:
      return "subtree";
    case PaintInvalidationReason::kSVGResource:
      return "SVG resource change";
    case PaintInvalidationReason::kCaret:
      return "caret";
    case PaintInvalidationReason::kDocumentMarker:
      return "DocumentMarker change";
    case PaintInvalidationReason::kImage:
      return "image";
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
    case PaintInvalidationReason::kForTesting:
      return "for testing";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& out, PaintInvalidationReason reason) {
  return out << PaintInvalidationReasonToString(reason);
}

}  // namespace blink
