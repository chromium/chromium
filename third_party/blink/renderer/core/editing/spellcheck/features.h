// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink::features {

// This experiment evaluates various restrictions on the application of
// spelling/grammar highlights to prevent user dictionary leaks.
// For more see:
// https://explainers-by-googlers.github.io/user-dictionary-leaks/
CORE_EXPORT BASE_DECLARE_FEATURE(kRestrictSpellingAndGrammarHighlights);

// If true, this disables spelling/grammar highlights performed on script
// edit (requiring user input to invoke).
CORE_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedContents);

// If true, this disables spelling/grammar highlights performed on script
// enablement (requiring contents or selection change).
CORE_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedEnablement);

// If true, this disables spelling/grammar highlights performed on script
// focus (requiring user gesture to invoke).
CORE_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedSelection);

}  // namespace blink::features

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_FEATURES_H_
