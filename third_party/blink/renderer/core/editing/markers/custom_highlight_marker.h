// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_CUSTOM_HIGHLIGHT_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_CUSTOM_HIGHLIGHT_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Highlight;

// A subclass of HighlightPseudoMarker for CSS custom highlights.
class CORE_EXPORT CustomHighlightMarker final : public HighlightPseudoMarker {
 public:
  CustomHighlightMarker(unsigned start_offset,
                        unsigned end_offset,
                        const String& highlight_name,
                        const Member<Highlight> highlight);
  CustomHighlightMarker(const CustomHighlightMarker&) = delete;
  CustomHighlightMarker& operator=(const CustomHighlightMarker&) = delete;

  MarkerType GetType() const final;
  PseudoId GetPseudoId() const final;
  const AtomicString& GetPseudoArgument() const final;

  const Highlight* GetHighlight() const { return highlight_; }
  const AtomicString& GetHighlightName() const { return highlight_name_; }

  void Trace(blink::Visitor*) const override;

 private:
  const AtomicString highlight_name_;
  const Member<Highlight> highlight_;
};

template <>
struct DowncastTraits<CustomHighlightMarker> {
  static bool AllowFrom(const DocumentMarker& marker) {
    return marker.GetType() == DocumentMarker::kCustomHighlight;
  }
  static bool AllowFrom(const HighlightPseudoMarker& marker) {
    return marker.GetType() == DocumentMarker::kCustomHighlight;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_CUSTOM_HIGHLIGHT_MARKER_H_
