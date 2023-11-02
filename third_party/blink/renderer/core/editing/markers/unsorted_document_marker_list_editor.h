// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_UNSORTED_DOCUMENT_MARKER_LIST_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_UNSORTED_DOCUMENT_MARKER_LIST_EDITOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class DocumentMarker;

// This class holds static utility methods to be used in DocumentMarkerList
// implementations that store potentially overlapping markers in unsorted order.
class CORE_EXPORT UnsortedDocumentMarkerListEditor final {
 public:
  using MarkerList = HeapVector<Member<DocumentMarker>>;

  // Returns true if a marker was moved, false otherwise.
  static bool MoveMarkers(MarkerList* src_list,
                          int length,
                          DocumentMarkerList* dst_list);

  // Returns true if a marker was removed, false otherwise.
  static bool RemoveMarkers(MarkerList*, unsigned start_offset, int length);

  // Returns true if a marker was shifted or removed, false otherwise.
  // If the text marked by a marker is changed by the edit, this method attempts
  // to keep the marker tracking the marked region rather than removing the
  // marker.
  static bool ShiftMarkersContentIndependent(MarkerList*,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length);

  // Returns the first marker in the specified MarkerList whose interior
  // overlaps overlap with the range [start_offset, end_offset], or null if
  // there is no such marker.
  // Note: since the markers aren't stored in order in an unsorted marker list,
  // the first marker found isn't necessarily going to be the first marker
  // ordered by start or end offset.
  static DocumentMarker* FirstMarkerIntersectingRange(const MarkerList&,
                                                      unsigned start_offset,
                                                      unsigned end_offset);

  // Returns all markers in the specified MarkerList whose interior overlaps
  // with the range [start_offset, end_offset].
  static HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      const MarkerList&,
      unsigned start_offset,
      unsigned end_offset);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_UNSORTED_DOCUMENT_MARKER_LIST_EDITOR_H_
