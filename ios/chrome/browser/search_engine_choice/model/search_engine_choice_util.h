// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_

#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"

class ChromeBrowserState;

// Utilities for the search engine choice screen.

// Whether or not the choice screen should be displayed for existing users.
// The parameter `app_started_via_external_intent` is used only if `promo`
// is set to search_engines::ChoicePromo::kDialog. The value is ignored for
// other promo types.
bool ShouldDisplaySearchEngineChoiceScreen(
    ChromeBrowserState& browser_state,
    search_engines::ChoicePromo promo,
    bool app_started_via_external_intent);

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
