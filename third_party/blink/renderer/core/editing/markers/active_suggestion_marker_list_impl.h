// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_ACTIVE_SUGGESTION_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_ACTIVE_SUGGESTION_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// Implementation of DocumentMarkerList for ActiveSuggestion markers.
// ActiveSuggestion markers are always inserted in order, aside from some
// potential oddball cases (e.g. splitting the marker list into two nodes, then
// undoing the split). This means we can keep the list in sorted order to do
// some operations more efficiently, while still being able to do inserts in
// O(1) time at the end of the list.
class CORE_EXPORT ActiveSuggestionMarkerListImpl final
    : public DocumentMarkerList {
 public:
  ActiveSuggestionMarkerListImpl() = default;
  ActiveSuggestionMarkerListImpl(const ActiveSuggestionMarkerListImpl&) =
      delete;
  ActiveSuggestionMarkerListImpl& operator=(
      const ActiveSuggestionMarkerListImpl&) = delete;

  // DocumentMarkerList implementations
  DocumentMarker::MarkerType MarkerType() const final;

  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  DocumentMarker* FirstMarkerIntersectingRange(
      wtf_size_t start_offset,
      wtf_size_t end_offset) const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      wtf_size_t start_offset,
      wtf_size_t end_offset) const final;

  bool MoveMarkers(wtf_size_t length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(wtf_size_t start_offset, wtf_size_t length) final;
  bool ShiftMarkers(const String& node_text,
                    wtf_size_t offset,
                    wtf_size_t old_length,
                    wtf_size_t new_length) final;

  void MergeOverlappingMarkers() final {}

  void Trace(Visitor*) const override;

 private:
  HeapVector<Member<DocumentMarker>> markers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_ACTIVE_SUGGESTION_MARKER_LIST_IMPL_H_
