// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_H_

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Highlight;

class CORE_EXPORT HighlightMarker final : public DocumentMarker {
 public:
  HighlightMarker(unsigned start_offset,
                  unsigned end_offset,
                  const String& highlight_name,
                  const Member<Highlight> highlight);
  HighlightMarker(const HighlightMarker&) = delete;
  HighlightMarker& operator=(const HighlightMarker&) = delete;

  MarkerType GetType() const final;
  const Highlight* GetHighlight() const { return highlight_; }
  const AtomicString& GetHighlightName() const { return highlight_name_; }

  void Trace(blink::Visitor*) const override;

 private:
  const AtomicString highlight_name_;
  const Member<Highlight> highlight_;
};

template <>
struct DowncastTraits<HighlightMarker> {
  static bool AllowFrom(const DocumentMarker& document_marker) {
    return document_marker.GetType() == DocumentMarker::kHighlight;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_H_
