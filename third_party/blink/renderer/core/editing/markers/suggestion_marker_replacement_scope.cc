// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_replacement_scope.h"

namespace blink {

bool SuggestionMarkerReplacementScope::currently_in_scope_ = false;

SuggestionMarkerReplacementScope::SuggestionMarkerReplacementScope() {
  DCHECK(!currently_in_scope_);
  currently_in_scope_ = true;
}

SuggestionMarkerReplacementScope::~SuggestionMarkerReplacementScope() {
  currently_in_scope_ = false;
}

// static
bool SuggestionMarkerReplacementScope::CurrentlyInScope() {
  return currently_in_scope_;
}

}  // namespace blink
