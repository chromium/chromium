// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/features.h"

namespace blink::features {

BASE_FEATURE(kRestrictSpellingAndGrammarHighlights,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedContents,
                   &kRestrictSpellingAndGrammarHighlights,
                   "changed_contents",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedEnablement,
                   &kRestrictSpellingAndGrammarHighlights,
                   "changed_enablement",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedSelection,
                   &kRestrictSpellingAndGrammarHighlights,
                   "changed_selection",
                   false);

}  // namespace blink::features
