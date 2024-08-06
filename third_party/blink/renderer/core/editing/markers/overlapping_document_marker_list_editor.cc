// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/overlapping_document_marker_list_editor.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

namespace blink {

void OverlappingDocumentMarkerListEditor::AddMarker(
    MarkerList* list,
    DocumentMarker* marker) {
  if (list->empty() || list->back()->StartOffset() <= marker->StartOffset()) {
    list->push_back(marker);
    return;
  }

  auto const pos = std::lower_bound(
      list->begin(), list->end(), marker,
      [](const Member<DocumentMarker>& marker_in_list,
         const DocumentMarker* marker_to_insert) {
        return marker_in_list->StartOffset() <= marker_to_insert->StartOffset();
      });

  list->insert(base::checked_cast<wtf_size_t>(pos - list->begin()), marker);
}


bool OverlappingDocumentMarkerListEditor::MoveMarkers(
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

    // Remove the marker if it is split by the edit.
    if (marker->EndOffset() > end_offset)
      continue;

    dst_list->Add(marker);
    did_move_marker = true;
  }

  *src_list = std::move(unmoved_markers);
  return did_move_marker;
}

bool OverlappingDocumentMarkerListEditor::RemoveMarkers(MarkerList* list,
                                                     unsigned start_offset,
                                                     int length) {
  // For overlapping markers, even if sorted, the quickest way to perform
  // this operation is to build a new list with the markers that aren't
  // being removed. Exploiting the sort is difficult because markers
  // may be nested. See
  // OverlappingDocumentMarkerListEditorTest.RemoveMarkersNestedOverlap
  // for an example.
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

bool OverlappingDocumentMarkerListEditor::ShiftMarkers(
    MarkerList* list,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // For an overlapping marker list, the quickest way to perform this operation is
  // to build a new list with the markers not removed by the shift. Note that
  // ComputeOffsetsAfterShift will move markers in such a way that they remain
  // sorted in StartOffset through this operation.
  bool did_shift_marker = false;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : *list) {
    std::optional<DocumentMarker::MarkerOffsets> result =
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

HeapVector<Member<DocumentMarker>>
OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
    const MarkerList& list,
    unsigned start_offset,
    unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);

  // Optimize finding the last possible overlapping marker, then iterate
  // only to there. We can't do better because overlaps may be nested, and
  // sorted on start does not imply sorted on end.
  auto const end_it =
      std::upper_bound(list.begin(), list.end(), end_offset,
                       [](unsigned end_offset, const DocumentMarker* marker) {
                         return end_offset <= marker->StartOffset();
                       });

  HeapVector<Member<DocumentMarker>> results;
  base::ranges::copy_if(
      list.begin(), end_it, std::back_inserter(results),
      [start_offset, end_offset](const DocumentMarker* marker) {
        return marker->StartOffset() < end_offset &&
               marker->EndOffset() > start_offset;
      });
  return results;
}

}  // namespace blink
