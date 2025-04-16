// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/glic_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType GlicMarkerListImpl::MarkerType() const {
  return DocumentMarker::kGlic;
}

void GlicMarkerListImpl::MergeOverlappingMarkers() {
  NOTREACHED();
}

}  // namespace blink
