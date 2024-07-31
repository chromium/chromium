// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/overlapping_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_replacement_scope.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

UChar32 GetCodePointAt(const String& text, wtf_size_t index) {
  UChar32 c;
  U16_GET(text, 0, index, text.length(), c);
  return c;
}

std::optional<DocumentMarker::MarkerOffsets>
ComputeOffsetsAfterNonSuggestionEditingOperating(const DocumentMarker& marker,
                                                 const String& node_text,
                                                 unsigned offset,
                                                 unsigned old_length,
                                                 unsigned new_length) {
  unsigned marker_start = marker.StartOffset();
  unsigned marker_end = marker.EndOffset();

  // Marked text was modified
  if (offset < marker_end && offset + old_length > marker_start)
    return {};

  // Text inserted/replaced immediately after the marker, remove marker if first
  // character is a (Unicode) letter or digit
  if (offset == marker_end && new_length > 0) {
    if (WTF::unicode::IsAlphanumeric(GetCodePointAt(node_text, offset)))
      return {};
    return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
  }

  // Text inserted/replaced immediately before the marker, remove marker if
  // first character is a (Unicode) letter or digit
  if (offset == marker_start && new_length > 0) {
    if (WTF::unicode::IsAlphanumeric(
            GetCodePointAt(node_text, offset + new_length - 1)))
      return {};
    return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
  }

  // Don't care if text was deleted immediately before or after the marker
  return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
}

}  // namespace

DocumentMarker::MarkerType SuggestionMarkerListImpl::MarkerType() const {
  return DocumentMarker::kSuggestion;
}

bool SuggestionMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void SuggestionMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(DocumentMarker::kSuggestion, marker->GetType());
  OverlappingDocumentMarkerListEditor::AddMarker(&markers_, marker);
}

void SuggestionMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& SuggestionMarkerListImpl::GetMarkers()
    const {
  return markers_;
}

DocumentMarker* SuggestionMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
SuggestionMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                   unsigned end_offset) const {
  return OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool SuggestionMarkerListImpl::MoveMarkers(int length,
                                           DocumentMarkerList* dst_list) {
  return OverlappingDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                          dst_list);
}

bool SuggestionMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                             int length) {
  return OverlappingDocumentMarkerListEditor::RemoveMarkers(
      &markers_, start_offset, length);
}

bool SuggestionMarkerListImpl::ShiftMarkers(const String& node_text,
                                            unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  if (SuggestionMarkerReplacementScope::CurrentlyInScope())
    return ShiftMarkersForSuggestionReplacement(offset, old_length, new_length);

  return ShiftMarkersForNonSuggestionEditingOperation(node_text, offset,
                                                      old_length, new_length);
}

bool SuggestionMarkerListImpl::ShiftMarkersForSuggestionReplacement(
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Since suggestion markers may overlap, the quickest way to perform
  // this operation is to build a new list with the markers not removed by the
  // shift.
  bool did_shift_marker = false;
  unsigned end_offset = offset + old_length;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : markers_) {
    // Markers that intersect the replacement range, but do not fully contain
    // it, should be removed.
    const bool marker_intersects_replacement_range =
        marker->StartOffset() < end_offset && marker->EndOffset() > offset;
    const bool marker_contains_replacement_range =
        marker->StartOffset() <= offset && marker->EndOffset() >= end_offset;

    if (marker_intersects_replacement_range &&
        !marker_contains_replacement_range) {
      did_shift_marker = true;
      continue;
    }

    std::optional<DocumentMarker::MarkerOffsets> result =
        marker->ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (result == std::nullopt) {
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

  markers_ = std::move(unremoved_markers);
  return did_shift_marker;
}

bool SuggestionMarkerListImpl::ShiftMarkersForNonSuggestionEditingOperation(
    const String& node_text,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Since suggestion markers may overlap, the quickest way to perform
  // this operation is to build a new list with the markers not removed by the
  // shift.
  bool did_shift_marker = false;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : markers_) {
    std::optional<DocumentMarker::MarkerOffsets> result =
        ComputeOffsetsAfterNonSuggestionEditingOperating(
            *marker, node_text, offset, old_length, new_length);
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

  markers_ = std::move(unremoved_markers);
  return did_shift_marker;
}

void SuggestionMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

bool SuggestionMarkerListImpl::RemoveMarkerByTag(int32_t tag) {
  wtf_size_t posn = 0;
  for (DocumentMarker* marker : markers_) {
    if (To<SuggestionMarker>(marker)->Tag() == tag) {
      markers_.EraseAt(posn, 1);
      return true;
    }
    posn++;
  }

  return false;
}

bool SuggestionMarkerListImpl::RemoveMarkerByType(
    const SuggestionMarker::SuggestionType& type) {
  wtf_size_t posn = 0;
  for (DocumentMarker* marker : markers_) {
    if (To<SuggestionMarker>(marker)->GetSuggestionType() == type) {
      markers_.EraseAt(posn, 1);
      return true;
    }
    posn++;
  }

  return false;
}

}  // namespace blink
