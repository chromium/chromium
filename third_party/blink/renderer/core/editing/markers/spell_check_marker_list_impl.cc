// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

namespace blink {

bool SpellCheckMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void SpellCheckMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(MarkerType(), marker->GetType());

  if (markers_.empty() ||
      markers_.back()->EndOffset() < marker->StartOffset()) {
    markers_.push_back(marker);
    return;
  }

  // Find first marker that ends after the one being inserted starts. If any
  // markers overlap the one being inserted, this is the first one.
  auto const first_overlapping = std::lower_bound(
      markers_.begin(), markers_.end(), marker,
      [](const Member<DocumentMarker>& marker_in_list,
         const DocumentMarker* marker_to_insert) {
        return marker_in_list->EndOffset() < marker_to_insert->StartOffset();
      });
  wtf_size_t first_overlapping_index =
      base::checked_cast<wtf_size_t>(first_overlapping - markers_.begin());

  // If this marker does not overlap the one being inserted, insert before it
  // and we are done.
  if (marker->EndOffset() < (*first_overlapping)->StartOffset()) {
    markers_.insert(first_overlapping_index, marker);
    return;
  }

  // Otherwise, find the last overlapping marker, replace the first marker with
  // the newly-inserted marker (to get the new description), set its start and
  // end offsets to include all the overlapped markers, and erase the rest of
  // the old markers.

  auto const last_overlapping = std::upper_bound(
      first_overlapping, markers_.end(), marker,
      [](const DocumentMarker* marker_to_insert,
         const Member<DocumentMarker>& marker_in_list) {
        return marker_to_insert->EndOffset() < marker_in_list->StartOffset();
      });
  wtf_size_t last_overlapping_index =
      base::checked_cast<wtf_size_t>(last_overlapping - markers_.begin());

  marker->SetStartOffset(
      std::min(marker->StartOffset(), (*first_overlapping)->StartOffset()));
  marker->SetEndOffset(std::max(
      marker->EndOffset(), markers_[last_overlapping_index - 1]->EndOffset()));

  *first_overlapping = marker;
  if (last_overlapping_index > first_overlapping_index + 1) {
    wtf_size_t num_to_erase =
        last_overlapping_index - (first_overlapping_index + 1);
    markers_.EraseAt(first_overlapping_index + 1, num_to_erase);
  }
}

void SpellCheckMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& SpellCheckMarkerListImpl::GetMarkers()
    const {
  return markers_;
}

DocumentMarker* SpellCheckMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
SpellCheckMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                   unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool SpellCheckMarkerListImpl::MoveMarkers(int length,
                                           DocumentMarkerList* dst_list) {
  return SortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                     dst_list);
}

bool SpellCheckMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                             int length) {
  return SortedDocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                       length);
}

bool SpellCheckMarkerListImpl::ShiftMarkers(const String&,
                                            unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  return SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(
      &markers_, offset, old_length, new_length);
}

void SpellCheckMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

bool SpellCheckMarkerListImpl::RemoveMarkersUnderWords(
    const String& node_text,
    const Vector<String>& words) {
  bool removed_markers = false;
  for (wtf_size_t j = markers_.size(); j > 0; --j) {
    const DocumentMarker& marker = *markers_[j - 1];
    const unsigned start = marker.StartOffset();
    const unsigned length = marker.EndOffset() - marker.StartOffset();
    const String& marker_text = node_text.Substring(start, length);
    if (words.Contains(marker_text)) {
      markers_.EraseAt(j - 1);
      removed_markers = true;
    }
  }
  return removed_markers;
}

}  // namespace blink
