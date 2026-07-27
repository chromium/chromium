// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_OVERLAPPING_DOCUMENT_MARKER_LIST_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_OVERLAPPING_DOCUMENT_MARKER_LIST_EDITOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class DocumentMarker;

// This class holds static utility methods to be used in DocumentMarkerList
// implementations that store potentially overlapping markers sorted by
// StartOffset. The sort order for markers with the same StartOffset is
// undefined. It will initially match the order in which markers are
// added, but calling ShiftMarkers may change that.
class CORE_EXPORT OverlappingDocumentMarkerListEditor final {
 public:
  using MarkerList = HeapVector<Member<DocumentMarker>>;

  static void AddMarker(MarkerList*, DocumentMarker*);

  // Returns true if a marker was moved, false otherwise.
  static bool MoveMarkers(MarkerList* src_list,
                          wtf_size_t length,
                          DocumentMarkerList* dst_list);

  // Returns true if a marker was removed, false otherwise.
  static bool RemoveMarkers(MarkerList*,
                            wtf_size_t start_offset,
                            wtf_size_t length);

  // Returns true if a marker was shifted or removed, false otherwise.
  // If the text marked by a marker is changed by the edit, this method attempts
  // to keep the marker tracking the marked region rather than removing the
  // marker.
  static bool ShiftMarkers(MarkerList*,
                           wtf_size_t offset,
                           wtf_size_t old_length,
                           wtf_size_t new_length);

  // Returns all markers in the specified MarkerList whose interior overlaps
  // with the range [start_offset, end_offset].
  static HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      const MarkerList&,
      wtf_size_t start_offset,
      wtf_size_t end_offset);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_OVERLAPPING_DOCUMENT_MARKER_LIST_EDITOR_H_
