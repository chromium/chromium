// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/text_marker_base_list_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Implementation of TextMarkerBaseListImpl for TextFragment markers.
class CORE_EXPORT TextFragmentMarkerListImpl final
    : public TextMarkerBaseListImpl {
 public:
  TextFragmentMarkerListImpl() = default;

  // DocumentMarkerList implementations
  DocumentMarker::MarkerType MarkerType() const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextFragmentMarkerListImpl);
};

template <>
struct DowncastTraits<TextFragmentMarkerListImpl> {
  static bool AllowFrom(const DocumentMarkerList& list) {
    return list.MarkerType() == DocumentMarker::kTextFragment;
  }
  static bool AllowFrom(const TextMarkerBaseListImpl& list) {
    return list.MarkerType() == DocumentMarker::kTextFragment;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_FRAGMENT_MARKER_LIST_IMPL_H_
