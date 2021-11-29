// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker_list_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Implementation of HighlightPseudoMarkerListImpl for Highlight markers.
class CORE_EXPORT HighlightMarkerListImpl final
    : public HighlightPseudoMarkerListImpl {
 public:
  HighlightMarkerListImpl() = default;
  HighlightMarkerListImpl(const HighlightMarkerListImpl&) = delete;
  HighlightMarkerListImpl& operator=(const HighlightMarkerListImpl&) = delete;

  DocumentMarker::MarkerType MarkerType() const final;
};

template <>
struct DowncastTraits<HighlightMarkerListImpl> {
  static bool AllowFrom(const DocumentMarkerList& list) {
    return list.MarkerType() == DocumentMarker::kHighlight;
  }
  static bool AllowFrom(const HighlightPseudoMarkerListImpl& list) {
    return list.MarkerType() == DocumentMarker::kHighlight;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_HIGHLIGHT_MARKER_LIST_IMPL_H_
