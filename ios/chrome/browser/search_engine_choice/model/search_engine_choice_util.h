// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_

class ProfileIOS;

// Utilities for the search engine choice screen.

// Maximum number of times the search engine choice screen can be skipped
// because the application is started via an external intent. Once this
// count is reached, the search engine choice screen is presented on all
// restarts until the user has made a decision.
constexpr int kSearchEngineChoiceMaximumSkipCount = 10;

// Whether or not the choice screen should be displayed for existing users.
// The parameter `app_started_via_external_intent` is used only if
// `is_first_run_entrypoint` is set to `false . The value is ignored otherwise.
bool ShouldDisplaySearchEngineChoiceScreen(
    ProfileIOS& profile,
    bool is_first_run_entrypoint,
    bool app_started_via_external_intent);

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
