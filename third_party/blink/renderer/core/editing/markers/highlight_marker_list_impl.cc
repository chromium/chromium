// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/highlight_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType HighlightMarkerListImpl::MarkerType() const {
  return DocumentMarker::kHighlight;
}

}  // namespace blink
