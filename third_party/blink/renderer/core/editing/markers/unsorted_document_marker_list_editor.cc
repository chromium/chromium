// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/unsorted_document_marker_list_editor.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

namespace blink {

bool UnsortedDocumentMarkerListEditor::MoveMarkers(
    MarkerList* src_list,
    int length,
    DocumentMarkerList* dst_list) {
  DCHECK_GT(length, 0);
  bool did_move_marker = false;
  const unsigned end_offset = length - 1;

  HeapVector<Member<DocumentMarker>> unmoved_markers;
  for (DocumentMarker* marker : *src_list) {
    if (marker->StartOffset() > end_offset) {
      unmoved_markers.push_back(marker);
      continue;
    }

    // If we're splitting a text node in the middle of a suggestion marker,
    // remove the marker
    if (marker->EndOffset() > end_offset)
      continue;

    dst_list->Add(marker);
    did_move_marker = true;
  }

  *src_list = std::move(unmoved_markers);
  return did_move_marker;
}

bool UnsortedDocumentMarkerListEditor::RemoveMarkers(MarkerList* list,
                                                     unsigned start_offset,
                                                     int length) {
  // For an unsorted marker list, the quickest way to perform this operation is
  // to build a new list with the markers that aren't being removed.
  const unsigned end_offset = start_offset + length;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : *list) {
    if (marker->EndOffset() <= start_offset ||
        marker->StartOffset() >= end_offset) {
      unremoved_markers.push_back(marker);
      continue;
    }
  }

  const bool did_remove_marker = (unremoved_markers.size() != list->size());
  *list = std::move(unremoved_markers);
  return did_remove_marker;
}

bool UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
    MarkerList* list,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // For an unsorted marker list, the quickest way to perform this operation is
  // to build a new list with the markers not removed by the shift.
  bool did_shift_marker = false;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : *list) {
    absl::optional<DocumentMarker::MarkerOffsets> result =
        marker->ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (!result) {
      did_shift_marker = true;
      continue;
    }

    if (marker->StartOffset() != result.value().start_offset ||
        marker->EndOffset() != result.value().end_offset) {
      marker->SetStartOffset(result.value().start_offset);
      marker->SetEndOffset(result.value().end_offset);
      did_shift_marker = true;
    }

    unremoved_markers.push_back(marker);
  }

  *list = std::move(unremoved_markers);
  return did_shift_marker;
}

DocumentMarker* UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
    const MarkerList& list,
    unsigned start_offset,
    unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);

  auto* const it = base::ranges::find_if(
      list, [start_offset, end_offset](const DocumentMarker* marker) {
        return marker->StartOffset() < end_offset &&
               marker->EndOffset() > start_offset;
      });

  if (it == list.end())
    return nullptr;
  return it->Get();
}

HeapVector<Member<DocumentMarker>>
UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(
    const MarkerList& list,
    unsigned start_offset,
    unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);

  HeapVector<Member<DocumentMarker>> results;
  base::ranges::copy_if(
      list, std::back_inserter(results),
      [start_offset, end_offset](const DocumentMarker* marker) {
        return marker->StartOffset() < end_offset &&
               marker->EndOffset() > start_offset;
      });
  return results;
}

}  // namespace blink
