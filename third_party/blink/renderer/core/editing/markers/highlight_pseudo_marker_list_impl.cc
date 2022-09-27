// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/unsorted_document_marker_list_editor.h"

namespace blink {

bool HighlightPseudoMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void HighlightPseudoMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK(marker->GetType() == DocumentMarker::kCustomHighlight ||
         marker->GetType() == DocumentMarker::kTextFragment);
  markers_.push_back(marker);
}

void HighlightPseudoMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
HighlightPseudoMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* HighlightPseudoMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
HighlightPseudoMarkerListImpl::MarkersIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool HighlightPseudoMarkerListImpl::MoveMarkers(
    int length,
    DocumentMarkerList* dst_markers_) {
  return UnsortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                       dst_markers_);
}

bool HighlightPseudoMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                                  int length) {
  return UnsortedDocumentMarkerListEditor::RemoveMarkers(&markers_,
                                                         start_offset, length);
}

bool HighlightPseudoMarkerListImpl::ShiftMarkers(const String&,
                                                 unsigned offset,
                                                 unsigned old_length,
                                                 unsigned new_length) {
  return UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
      &markers_, offset, old_length, new_length);
}

void HighlightPseudoMarkerListImpl::Trace(blink::Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
