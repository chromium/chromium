// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"

namespace blink {

GrammarMarker::GrammarMarker(unsigned start_offset,
                             unsigned end_offset,
                             const String& description,
                             bool should_hide_suggestion_menu)
    : SpellCheckMarker(start_offset,
                       end_offset,
                       description,
                       should_hide_suggestion_menu) {
  DCHECK_LT(start_offset, end_offset);
}

DocumentMarker::MarkerType GrammarMarker::GetType() const {
  return DocumentMarker::kGrammar;
}

}  // namespace blink
