// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_spelling_marker.h"

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"

namespace blink {

WebSpellingMarker::WebSpellingMarker(const DocumentMarker& marker)
    : start(marker.StartOffset()), end(marker.EndOffset()) {
  if (marker.GetType() == DocumentMarker::kSpelling) {
    marker_type = SpellingMarkerType::kSpelling;
  } else if (marker.GetType() == DocumentMarker::kGrammar) {
    marker_type = SpellingMarkerType::kGrammar;
  } else if (const auto* suggestion_marker =
                 DynamicTo<SuggestionMarker>(marker)) {
    marker_type = suggestion_marker->IsGrammarError()
                      ? SpellingMarkerType::kGrammar
                      : SpellingMarkerType::kSpelling;
  } else {
    NOTREACHED();
  }
}

}  // namespace blink
