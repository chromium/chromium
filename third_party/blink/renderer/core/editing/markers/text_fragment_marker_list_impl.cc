// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker_list_impl.h"

namespace blink {

DocumentMarker::MarkerType TextFragmentMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextFragment;
}

}  // namespace blink
