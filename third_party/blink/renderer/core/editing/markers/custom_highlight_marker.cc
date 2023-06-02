// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"

#include "third_party/blink/renderer/core/highlight/highlight.h"

namespace blink {

CustomHighlightMarker::CustomHighlightMarker(unsigned start_offset,
                                             unsigned end_offset,
                                             const String& highlight_name,
                                             const Member<Highlight> highlight)
    : HighlightPseudoMarker(start_offset, end_offset),
      highlight_name_(highlight_name),
      highlight_(highlight) {}

DocumentMarker::MarkerType CustomHighlightMarker::GetType() const {
  return DocumentMarker::kCustomHighlight;
}

PseudoId CustomHighlightMarker::GetPseudoId() const {
  return kPseudoIdHighlight;
}

const AtomicString& CustomHighlightMarker::GetPseudoArgument() const {
  return GetHighlightName();
}

void CustomHighlightMarker::SetHasVisualOverflow(bool has_overflow) {
  highlight_has_visual_overflow_ = has_overflow;
}

bool CustomHighlightMarker::HasVisualOverflow() const {
  return highlight_has_visual_overflow_;
}

void CustomHighlightMarker::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_);
  DocumentMarker::Trace(visitor);
}

}  // namespace blink
