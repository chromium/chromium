// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_REGISTRATION_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_REGISTRATION_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// Registers the prefs needed by voice search.
void RegisterVoiceSearchBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_REGISTRATION_H_
