// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

namespace blink {

DocumentMarker::MarkerType ActiveSuggestionMarkerListImpl::MarkerType() const {
  return DocumentMarker::kActiveSuggestion;
}

bool ActiveSuggestionMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void ActiveSuggestionMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(DocumentMarker::kActiveSuggestion, marker->GetType());
  SortedDocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                                     marker);
}

void ActiveSuggestionMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
ActiveSuggestionMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* ActiveSuggestionMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
ActiveSuggestionMarkerListImpl::MarkersIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool ActiveSuggestionMarkerListImpl::MoveMarkers(
    int length,
    DocumentMarkerList* dst_markers_) {
  return SortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                     dst_markers_);
}

bool ActiveSuggestionMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                                   int length) {
  return SortedDocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                       length);
}

bool ActiveSuggestionMarkerListImpl::ShiftMarkers(const String&,
                                                  unsigned offset,
                                                  unsigned old_length,
                                                  unsigned new_length) {
  return SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
      &markers_, offset, old_length, new_length);
}

void ActiveSuggestionMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
