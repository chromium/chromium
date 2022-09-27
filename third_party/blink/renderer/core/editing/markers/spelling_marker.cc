// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"

namespace blink {

SpellingMarker::SpellingMarker(unsigned start_offset,
                               unsigned end_offset,
                               const String& description)
    : SpellCheckMarker(start_offset, end_offset, description) {
  DCHECK_LT(start_offset, end_offset);
}

DocumentMarker::MarkerType SpellingMarker::GetType() const {
  return DocumentMarker::kSpelling;
}

}  // namespace blink
