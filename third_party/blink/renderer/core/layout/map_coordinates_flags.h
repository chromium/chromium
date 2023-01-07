// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_

namespace blink {

enum MapCoordinatesMode {
  // Only needed in some special cases to intentionally ignore transforms.
  kIgnoreTransforms = 1 << 2,

  kTraverseDocumentBoundaries = 1 << 3,

  // Ignore offset adjustments caused by position:sticky calculations when
  // walking the chain.
  kIgnoreStickyOffset = 1 << 4,

  // Ignore scroll offset from container, i.e. scrolling has no effect on mapped
  // position.
  kIgnoreScrollOffset = 1 << 5,

  // If the local root frame has a remote frame parent, apply the transformation
  // from the local root frame to the remote main frame.
  kApplyRemoteMainFrameTransform = 1 << 6,
};
typedef unsigned MapCoordinatesFlags;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MAP_COORDINATES_FLAGS_H_
