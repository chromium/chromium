// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MATCH_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MATCH_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/text_marker_base_list_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class IntRect;

// Implementation of TextMarkerBaseListImpl for TextMatch markers.
// Markers are kept sorted by start offset, under the assumption that
// TextMatch markers are typically inserted in an order.
class CORE_EXPORT TextMatchMarkerListImpl final
    : public TextMarkerBaseListImpl {
 public:
  TextMatchMarkerListImpl() = default;

  // DocumentMarkerList implementations
  DocumentMarker::MarkerType MarkerType() const final;

  // TextMatchMarkerListImpl-specific
  Vector<IntRect> LayoutRects(const Node&) const;
  // Returns true if markers within a range defined by |startOffset| and
  // |endOffset| are found.
  bool SetTextMatchMarkersActive(unsigned start_offset,
                                 unsigned end_offset,
                                 bool);

 private:
  DISALLOW_COPY_AND_ASSIGN(TextMatchMarkerListImpl);
};

template <>
struct DowncastTraits<TextMatchMarkerListImpl> {
  static bool AllowFrom(const DocumentMarkerList& list) {
    return list.MarkerType() == DocumentMarker::kTextMatch;
  }
  static bool AllowFrom(const TextMarkerBaseListImpl& list) {
    return list.MarkerType() == DocumentMarker::kTextMatch;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MATCH_MARKER_LIST_IMPL_H_
