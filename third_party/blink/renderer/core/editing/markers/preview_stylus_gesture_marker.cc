// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/preview_stylus_gesture_marker.h"

#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

PreviewStylusGestureMarker::PreviewStylusGestureMarker(unsigned start_offset,
                                                       unsigned end_offset,
                                                       Color background_color)
    : StyleableMarker(start_offset,
                      end_offset,
                      Color::kTransparent,
                      ui::mojom::blink::ImeTextSpanThickness::kNone,
                      ui::mojom::blink::ImeTextSpanUnderlineStyle::kNone,
                      Color::kTransparent,
                      background_color) {}

DocumentMarker::MarkerType PreviewStylusGestureMarker::GetType() const {
  return DocumentMarker::kPreviewStylusGesture;
}

}  // namespace blink
