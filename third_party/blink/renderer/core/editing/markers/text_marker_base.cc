// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_marker_base.h"

namespace blink {

TextMarkerBase::TextMarkerBase(unsigned start_offset, unsigned end_offset)
    : DocumentMarker(start_offset, end_offset) {}

bool IsTextMarker(const DocumentMarker& marker) {
  return marker.GetType() == DocumentMarker::kTextMatch ||
         marker.GetType() == DocumentMarker::kTextFragment;
}

}  // namespace blink
