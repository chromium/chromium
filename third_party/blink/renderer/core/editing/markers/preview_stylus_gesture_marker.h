// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_PREVIEW_STYLUS_GESTURE_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_PREVIEW_STYLUS_GESTURE_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"

namespace blink {

// A subclass of DocumentMarker used to store information specific to
// stylus writing. We store the highlight color used to preview
// the Select and Delete handwriting gestures.
class CORE_EXPORT PreviewStylusGestureMarker final : public StyleableMarker {
 public:
  PreviewStylusGestureMarker(unsigned start_offset,
                             unsigned end_offset,
                             Color background_color);
  PreviewStylusGestureMarker(const PreviewStylusGestureMarker&) = delete;
  PreviewStylusGestureMarker& operator=(const PreviewStylusGestureMarker&) =
      delete;

  // DocumentMarker implementations
  MarkerType GetType() const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_PREVIEW_STYLUS_GESTURE_MARKER_H_
