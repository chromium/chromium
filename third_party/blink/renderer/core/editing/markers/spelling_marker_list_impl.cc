// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spelling_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType SpellingMarkerListImpl::MarkerType() const {
  return DocumentMarker::kSpelling;
}

}  // namespace blink
