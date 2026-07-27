// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/preview_stylus_gesture_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/overlapping_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

namespace {

bool IsIntersecting(
    const blink::HeapVector<blink::Member<blink::DocumentMarker>>& markers,
    blink::wtf_size_t start_offset,
    blink::wtf_size_t end_offset) {
  if (!markers.empty()) {  // Check if NOT empty
    return markers[0]->StartOffset() < end_offset &&
           markers[0]->EndOffset() > start_offset;
  }
  return false;
}

}  // namespace

namespace blink {

DocumentMarker::MarkerType PreviewStylusGestureMarkerListImpl::MarkerType()
    const {
  return DocumentMarker::kPreviewStylusGesture;
}

bool PreviewStylusGestureMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void PreviewStylusGestureMarkerListImpl::Add(DocumentMarker* marker) {
  CHECK_EQ(DocumentMarker::kPreviewStylusGesture, marker->GetType());
  if (!IsEmpty()) {
    Clear();
  }
  OverlappingDocumentMarkerListEditor::AddMarker(&markers_, marker);
}

void PreviewStylusGestureMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
PreviewStylusGestureMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker*
PreviewStylusGestureMarkerListImpl::FirstMarkerIntersectingRange(
    wtf_size_t start_offset,
    wtf_size_t end_offset) const {
  if (IsIntersecting(markers_, start_offset, end_offset)) {
    return markers_[0].Get();
  } else {
    return nullptr;
  }
}

HeapVector<Member<DocumentMarker>>
PreviewStylusGestureMarkerListImpl::MarkersIntersectingRange(
    wtf_size_t start_offset,
    wtf_size_t end_offset) const {
  if (IsIntersecting(markers_, start_offset, end_offset)) {
    return markers_;
  } else {
    HeapVector<Member<DocumentMarker>> empty = {};
    return empty;
  }
}

bool PreviewStylusGestureMarkerListImpl::MoveMarkers(
    wtf_size_t length,
    DocumentMarkerList* dst_markers_) {
  return OverlappingDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                          dst_markers_);
}

bool PreviewStylusGestureMarkerListImpl::RemoveMarkers(wtf_size_t start_offset,
                                                       wtf_size_t length) {
  return OverlappingDocumentMarkerListEditor::RemoveMarkers(
      &markers_, start_offset, length);
}

bool PreviewStylusGestureMarkerListImpl::ShiftMarkers(const String&,
                                                      wtf_size_t offset,
                                                      wtf_size_t old_length,
                                                      wtf_size_t new_length) {
  return OverlappingDocumentMarkerListEditor::ShiftMarkers(
      &markers_, offset, old_length, new_length);
}

void PreviewStylusGestureMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
