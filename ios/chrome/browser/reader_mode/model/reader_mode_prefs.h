// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PREFS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace reader_mode_prefs {

// Pref holding the latest timestamps of when the user has
// interacted with Reading Mode.
extern const char kReaderModeRecentlyUsedTimestampsPref[];

// Registers the prefs associated with Reading Mode usage.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the user meets the criteria for having
// recently used the Reading Mode feature.
bool IsReaderModeRecentlyUsed(const PrefService& prefs);

}  // namespace reader_mode_prefs

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PREFS_H_
