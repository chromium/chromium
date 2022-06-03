// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"

namespace blink {

ActiveSuggestionMarker::ActiveSuggestionMarker(
    unsigned start_offset,
    unsigned end_offset,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    ui::mojom::ImeTextSpanUnderlineStyle underline_style,
    Color text_color,
    Color background_color)
    : StyleableMarker(start_offset,
                      end_offset,
                      underline_color,
                      thickness,
                      underline_style,
                      text_color,
                      background_color) {}

DocumentMarker::MarkerType ActiveSuggestionMarker::GetType() const {
  return DocumentMarker::kActiveSuggestion;
}

}  // namespace blink
