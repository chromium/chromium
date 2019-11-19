// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_marker_base_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

namespace blink {

bool TextMarkerBaseListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void TextMarkerBaseListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(marker->GetType(), MarkerType());
  SortedDocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                                     marker);
}

void TextMarkerBaseListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& TextMarkerBaseListImpl::GetMarkers()
    const {
  return markers_;
}

DocumentMarker* TextMarkerBaseListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
TextMarkerBaseListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                 unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool TextMarkerBaseListImpl::MoveMarkers(int length,
                                         DocumentMarkerList* dst_list) {
  return SortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                     dst_list);
}

bool TextMarkerBaseListImpl::RemoveMarkers(unsigned start_offset, int length) {
  return SortedDocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                       length);
}

bool TextMarkerBaseListImpl::ShiftMarkers(const String&,
                                          unsigned offset,
                                          unsigned old_length,
                                          unsigned new_length) {
  return SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(
      &markers_, offset, old_length, new_length);
}

void TextMarkerBaseListImpl::Trace(Visitor* visitor) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
