// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"

namespace blink {

SpellCheckMarker::SpellCheckMarker(unsigned start_offset,
                                   unsigned end_offset,
                                   const String& description,
                                   bool should_hide_suggestion_menu)
    : DocumentMarker(start_offset, end_offset),
      description_(description),
      should_hide_suggestion_menu_(should_hide_suggestion_menu) {
  DCHECK_LT(start_offset, end_offset);
}

bool IsSpellCheckMarker(const DocumentMarker& marker) {
  DocumentMarker::MarkerType type = marker.GetType();
  return type == DocumentMarker::kSpelling || type == DocumentMarker::kGrammar;
}

}  // namespace blink
