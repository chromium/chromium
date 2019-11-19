// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_H_

#include "third_party/blink/renderer/core/editing/markers/text_marker_base.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// A subclass of TextMarkerBase used for indicating a text fragment on the
// page. See blink/renderer/core/page/scrolling/text_fragment_anchor.h.
class CORE_EXPORT TextFragmentMarker final : public TextMarkerBase {
 public:
  TextFragmentMarker(unsigned start_offset, unsigned end_offset);

  // DocumentMarker implementations
  MarkerType GetType() const final;

  // TextMarkerBase implementations
  bool IsActiveMatch() const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextFragmentMarker);
};

template <>
struct DowncastTraits<TextFragmentMarker> {
  static bool AllowFrom(const DocumentMarker& marker) {
    return marker.GetType() == DocumentMarker::kTextFragment;
  }
  static bool AllowFrom(const TextMarkerBase& marker) {
    return marker.GetType() == DocumentMarker::kTextFragment;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_H_
