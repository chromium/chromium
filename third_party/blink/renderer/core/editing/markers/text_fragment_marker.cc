// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"

namespace blink {

TextFragmentMarker::TextFragmentMarker(unsigned start_offset,
                                       unsigned end_offset)
    : HighlightPseudoMarker(start_offset, end_offset) {}

DocumentMarker::MarkerType TextFragmentMarker::GetType() const {
  return DocumentMarker::kTextFragment;
}

PseudoId TextFragmentMarker::GetPseudoId() const {
  return kPseudoIdTargetText;
}

const AtomicString& TextFragmentMarker::GetPseudoArgument() const {
  return g_null_atom;
}

}  // namespace blink
