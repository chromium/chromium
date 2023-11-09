// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

DocumentMarker::MarkerType CustomHighlightMarkerListImpl::MarkerType() const {
  return DocumentMarker::kCustomHighlight;
}

void CustomHighlightMarkerListImpl::MergeOverlappingMarkers() {
  HeapVector<Member<DocumentMarker>> merged_markers;

  using NameToCustomHighlightMarkerMap =
      HeapHashMap<String, Member<CustomHighlightMarker>>;
  NameToCustomHighlightMarkerMap name_to_last_custom_highlight_marker_seen;

  for (auto& current_marker : markers_) {
    CustomHighlightMarker* current_custom_highlight_marker =
        To<CustomHighlightMarker>(current_marker.Get());

    NameToCustomHighlightMarkerMap::AddResult insert_result =
        name_to_last_custom_highlight_marker_seen.insert(
            current_custom_highlight_marker->GetHighlightName(),
            current_custom_highlight_marker);

    if (!insert_result.is_new_entry) {
      CustomHighlightMarker* stored_custom_highlight_marker =
          insert_result.stored_value->value;
      if (current_custom_highlight_marker->StartOffset() >=
          stored_custom_highlight_marker->EndOffset()) {
        // Markers don't intersect, so add the new one and mark it as current
        merged_markers.push_back(current_custom_highlight_marker);
        insert_result.stored_value->value = current_custom_highlight_marker;
      } else {
        // Markers overlap, so expand the stored marker to cover both and
        // discard the current one.
        stored_custom_highlight_marker->SetEndOffset(
            std::max(stored_custom_highlight_marker->EndOffset(),
                     current_custom_highlight_marker->EndOffset()));
      }
    } else {
      merged_markers.push_back(current_custom_highlight_marker);
    }
  }

  markers_ = std::move(merged_markers);
}

}  // namespace blink
