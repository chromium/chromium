// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_

class Browser;

// Utilities for the search engine choice screen.

// Whether or not the choice screen should be displayed for existing users.
bool ShouldDisplaySearchEngineChoiceScreen(Browser* browser);

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_UTIL_H_
