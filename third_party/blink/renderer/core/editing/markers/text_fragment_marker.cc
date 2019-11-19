// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"

namespace blink {

TextFragmentMarker::TextFragmentMarker(unsigned start_offset,
                                       unsigned end_offset)
    : TextMarkerBase(start_offset, end_offset) {
  DCHECK_LT(start_offset, end_offset);
}

DocumentMarker::MarkerType TextFragmentMarker::GetType() const {
  return DocumentMarker::kTextFragment;
}

bool TextFragmentMarker::IsActiveMatch() const {
  // The TextFragmentMarker is painted as an inactive text match marker.
  return false;
}

}  // namespace blink
