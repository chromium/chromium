// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker.h"

namespace blink {

HighlightPseudoMarker::HighlightPseudoMarker(unsigned start_offset,
                                             unsigned end_offset)
    : DocumentMarker(start_offset, end_offset) {}

}  // namespace blink
