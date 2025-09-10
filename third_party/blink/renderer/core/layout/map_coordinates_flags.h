// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_

namespace blink {

// Coordinate space overview (for cross-frame/OOPIF correctness):
//
// - Document space: Coordinates relative to the top-left of the main frame's
//   content (i.e., the main document origin). These coordinates are stable
//   across root scrolling. Use this when you want a scroll-invariant position
//   in the main document.
//
// - Viewport space: Coordinates relative to the top-left of the window
//   rendering the main document. These coordinates move with scrolling.
//
// Traversal across document boundaries:
// - Within-process frames (non-OOPIF): kTraverseDocumentBoundaries lets
//   mapping walk through the embedder's layout chain.
// - Cross-process frames (OOPIF): there is no in-process owner layout to walk.
//   Combine kTraverseDocumentBoundaries with one of the remote transforms
//   (kApplyRemoteMainFrameTransform or kApplyRemoteViewportTransform) to map
//   into the main frame's space.
//
// Recommended combinations (common intents):
// - Map to main document (scroll-invariant):
//     kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform |
//     kIgnoreScrollOriginAndOffset
// - Map to main viewport (scroll-dependent):
//     kTraverseDocumentBoundaries | kApplyRemoteViewportTransform
// - Ignore a container's scroll entirely while mapping within a single
//   document:
//     kIgnoreScrollOriginAndOffset (or kIgnoreScrollOffset for offset only)
//
// Usage notes:
// - Some mapping APIs disallow certain flags (e.g., AbsoluteBoundingBoxRect()
//   DCHECKs if kIgnoreTransforms is set). Refer to the method documentation.
// - Remote viewport mapping can be stale if the iframe is offscreen; see note
//   on kApplyRemoteViewportTransform below.
//
enum MapCoordinatesMode {
  // Only needed in some special cases to intentionally ignore transforms.
  kIgnoreTransforms = 1 << 2,

  kTraverseDocumentBoundaries = 1 << 3,

  // Ignore offset adjustments caused by position:sticky calculations when
  // walking the chain.
  kIgnoreStickyOffset = 1 << 4,

  // Ignore the dynamic scroll offset from the container (i.e., the amount
  // scrolled). The static ScrollOrigin (which can be non-zero in some writing
  // modes/directions) is still applied. This yields scroll-invariant mapping
  // while preserving writing-mode anchoring.
  kIgnoreScrollOffset = 1 << 5,

  // Ignore both the static ScrollOrigin and the dynamic scroll offset. This
  // produces a canonical top-left content origin regardless of writing mode or
  // scroll state.
  // (In default horizontal-tb LTR, ScrollOrigin is (0,0), so this is
  // equivalent to kIgnoreScrollOffset.)
  kIgnoreScrollOriginAndOffset = 1 << 6,

  // If the local root frame has a remote frame parent (i.e., this frame is
  // embedded via an out-of-process iframe), apply the transformation from the
  // local root frame to the "remote" main frame (remote here means
  // different-process, not a different machine). The resulting coordinates are
  // relative to the main frame document (content origin), i.e., (0, 0) maps to
  // where the main frame's content starts.
  kApplyRemoteMainFrameTransform = 1 << 7,

  // If the local root frame has a remote frame parent, apply the transformation
  // from the local root frame to the remote main frame's viewport (again,
  // remote means different-process, not a different machine). In this case
  // (0, 0) maps to the window/viewport origin of the embedder main Document,
  // i.e., scroll-dependent coordinates in the embedding page.
  //
  // NOTE: This is guaranteed to provide a correct value only if the iframe is
  // onscreen. This is because we don't sync scroll updates from the main
  // frame's root scroller. See kSkipUnnecessaryRemoteFrameGeometryPropagation.
  kApplyRemoteViewportTransform = 1 << 8,
};
typedef unsigned MapCoordinatesFlags;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_
