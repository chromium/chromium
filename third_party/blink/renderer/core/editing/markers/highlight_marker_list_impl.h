// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"

namespace blink {

// Implementation of HighlightMarkerList for Highlight markers.
class CORE_EXPORT HighlightMarkerListImpl final : public DocumentMarkerList {
 public:
  HighlightMarkerListImpl() = default;
  HighlightMarkerListImpl(const HighlightMarkerListImpl&) = delete;
  HighlightMarkerListImpl& operator=(const HighlightMarkerListImpl&) = delete;

  DocumentMarker::MarkerType MarkerType() const final;

  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  DocumentMarker* FirstMarkerIntersectingRange(unsigned start_offset,
                                               unsigned end_offset) const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      unsigned start_offset,
      unsigned end_offset) const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool ShiftMarkers(const String& node_text,
                    unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  void Trace(blink::Visitor*) const override;

 private:
  HeapVector<Member<DocumentMarker>> markers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_
