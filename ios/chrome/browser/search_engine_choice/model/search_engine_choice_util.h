// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Utilities for the search engine choice screen.

// Whether or not the choice screen should be displayed for existing users.
// The parameter `app_started_via_external_intent` is used only if
// `is_first_run_entrypoint` is set to `false . The value is ignored otherwise.
bool ShouldDisplaySearchEngineChoiceScreen(
    ProfileIOS& profile,
    bool is_first_run_entrypoint,
    bool app_started_via_external_intent);

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
