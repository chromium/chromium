// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType TextFragmentMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextFragment;
}

void TextFragmentMarkerListImpl::MergeOverlappingMarkers() {
  HeapVector<Member<DocumentMarker>> merged_markers;
  DocumentMarker* active_marker = nullptr;

  for (auto& marker : markers_) {
    if (!active_marker || marker->StartOffset() >= active_marker->EndOffset()) {
      // Markers don't intersect, so add the new one and mark it as current
      merged_markers.push_back(marker);
      active_marker = marker;
    } else {
      // Markers overlap, so expand the active marker to cover both and
      // discard the current one.
      active_marker->SetEndOffset(
          std::max(active_marker->EndOffset(), marker->EndOffset()));
    }
  }
  markers_ = std::move(merged_markers);
}

}  // namespace blink
