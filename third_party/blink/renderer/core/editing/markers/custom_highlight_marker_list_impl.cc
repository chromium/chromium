// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType CustomHighlightMarkerListImpl::MarkerType() const {
  return DocumentMarker::kCustomHighlight;
}

}  // namespace blink
