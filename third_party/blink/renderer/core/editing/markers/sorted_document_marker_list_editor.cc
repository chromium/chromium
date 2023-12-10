// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

namespace blink {

void SortedDocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(
    MarkerList* list,
    DocumentMarker* marker) {
  if (list->empty() || list->back()->EndOffset() <= marker->StartOffset()) {
    list->push_back(marker);
    return;
  }

  auto* const pos = std::lower_bound(
      list->begin(), list->end(), marker,
      [](const Member<DocumentMarker>& marker_in_list,
         const DocumentMarker* marker_to_insert) {
        return marker_in_list->StartOffset() < marker_to_insert->StartOffset();
      });

  // DCHECK that we're not trying to add a marker that overlaps an existing one
  // (this method only works for lists which don't allow overlapping markers)
  if (pos != list->end())
    DCHECK_LE(marker->EndOffset(), (*pos)->StartOffset());

  if (pos != list->begin())
    DCHECK_GE(marker->StartOffset(), (*std::prev(pos))->EndOffset());

  list->insert(base::checked_cast<wtf_size_t>(pos - list->begin()), marker);
}

bool SortedDocumentMarkerListEditor::MoveMarkers(MarkerList* src_list,
                                                 int length,
                                                 DocumentMarkerList* dst_list) {
  DCHECK_GT(length, 0);
  bool didMoveMarker = false;
  unsigned end_offset = length - 1;

  MarkerList::iterator it;
  for (it = src_list->begin(); it != src_list->end(); ++it) {
    DocumentMarker& marker = **it;
    if (marker.StartOffset() > end_offset)
      break;

    // Trim the marker to fit in dst_list's text node
    if (marker.EndOffset() > end_offset)
      marker.SetEndOffset(end_offset);

    dst_list->Add(&marker);
    didMoveMarker = true;
  }

  // Remove the range of markers that were moved to dstNode
  src_list->EraseAt(0, base::checked_cast<wtf_size_t>(it - src_list->begin()));

  return didMoveMarker;
}

bool SortedDocumentMarkerListEditor::RemoveMarkers(MarkerList* list,
                                                   unsigned start_offset,
                                                   int length) {
  const unsigned end_offset = start_offset + length;
  MarkerList::iterator start_pos = std::upper_bound(
      list->begin(), list->end(), start_offset,
      [](size_t start_offset, const Member<DocumentMarker>& marker) {
        return start_offset < marker->EndOffset();
      });

  MarkerList::iterator end_pos = std::lower_bound(
      list->begin(), list->end(), end_offset,
      [](const Member<DocumentMarker>& marker, size_t end_offset) {
        return marker->StartOffset() < end_offset;
      });

  list->EraseAt(base::checked_cast<wtf_size_t>(start_pos - list->begin()),
                base::checked_cast<wtf_size_t>(end_pos - start_pos));
  return start_pos != end_pos;
}

bool SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(
    MarkerList* list,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Find first marker that ends after the start of the region being edited.
  // Markers before this one can be left untouched. This saves us some time over
  // scanning the entire list linearly if the edit region is near the end of the
  // text node.
  const MarkerList::iterator& shift_range_begin =
      std::upper_bound(list->begin(), list->end(), offset,
                       [](size_t offset, const Member<DocumentMarker>& marker) {
                         return offset < marker->EndOffset();
                       });

  MarkerList::iterator erase_range_end = shift_range_begin;

  bool did_shift_marker = false;
  for (MarkerList::iterator it = shift_range_begin; it != list->end(); ++it) {
    DocumentMarker& marker = **it;

    // marked text is (potentially) changed by edit, remove marker
    if (marker.StartOffset() < offset + old_length) {
      erase_range_end = std::next(it);
      did_shift_marker = true;
      continue;
    }

    // marked text is shifted but not changed
    marker.ShiftOffsets(new_length - old_length);
    did_shift_marker = true;
  }

  // Note: shift_range_begin could point at a marker being shifted instead of
  // deleted, but if this is the case, we don't need to delete any markers, and
  // EraseAt() will get 0 for the length param
  list->EraseAt(
      base::checked_cast<wtf_size_t>(shift_range_begin - list->begin()),
      base::checked_cast<wtf_size_t>(erase_range_end - shift_range_begin));
  return did_shift_marker;
}

bool SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
    MarkerList* list,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Find first marker that ends after the start of the region being edited.
  // Markers before this one can be left untouched. This saves us some time over
  // scanning the entire list linearly if the edit region is near the end of the
  // text node.
  const MarkerList::iterator& shift_range_begin =
      std::upper_bound(list->begin(), list->end(), offset,
                       [](size_t offset, const Member<DocumentMarker>& marker) {
                         return offset < marker->EndOffset();
                       });

  MarkerList::iterator erase_range_begin = list->end();
  MarkerList::iterator erase_range_end = list->end();

  bool did_shift_marker = false;
  for (MarkerList::iterator it = shift_range_begin; it != list->end(); ++it) {
    DocumentMarker& marker = **it;
    std::optional<DocumentMarker::MarkerOffsets> result =
        marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (result == std::nullopt) {
      if (erase_range_begin == list->end())
        erase_range_begin = it;
      erase_range_end = std::next(it);
      did_shift_marker = true;
      continue;
    }

    if (marker.StartOffset() != result.value().start_offset ||
        marker.EndOffset() != result.value().end_offset) {
      did_shift_marker = true;
      marker.SetStartOffset(result.value().start_offset);
      marker.SetEndOffset(result.value().end_offset);
    }
  }

  list->EraseAt(
      base::checked_cast<wtf_size_t>(erase_range_begin - list->begin()),
      base::checked_cast<wtf_size_t>(erase_range_end - erase_range_begin));
  return did_shift_marker;
}

DocumentMarker* SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
    const MarkerList& list,
    unsigned start_offset,
    unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);

  auto* const marker_it =
      std::lower_bound(list.begin(), list.end(), start_offset,
                       [](const DocumentMarker* marker, unsigned start_offset) {
                         return marker->EndOffset() <= start_offset;
                       });
  if (marker_it == list.end())
    return nullptr;

  DocumentMarker* marker = *marker_it;
  if (marker->StartOffset() >= end_offset)
    return nullptr;
  return marker;
}

HeapVector<Member<DocumentMarker>>
SortedDocumentMarkerListEditor::MarkersIntersectingRange(const MarkerList& list,
                                                         unsigned start_offset,
                                                         unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);

  auto* const start_it =
      std::lower_bound(list.begin(), list.end(), start_offset,
                       [](const DocumentMarker* marker, unsigned start_offset) {
                         return marker->EndOffset() <= start_offset;
                       });
  auto* const end_it =
      std::upper_bound(list.begin(), list.end(), end_offset,
                       [](unsigned end_offset, const DocumentMarker* marker) {
                         return end_offset <= marker->StartOffset();
                       });

  HeapVector<Member<DocumentMarker>> results;
  std::copy(start_it, end_it, std::back_inserter(results));
  return results;
}

}  // namespace blink
