// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked/pref_names.h"

namespace user_prefs {

// A timestamp (stored in base::Time::ToInternalValue format) of the last time
// a preference was reset.
const char kPreferenceResetTime[] = "prefs.preference_reset_time";

// A dictionary of all the tracked preferences that have been reset.
const char kTrackedPreferencesReset[] = "prefs.tracked_preferences_reset";

// Pref that can be set to trigger a write of the preference file to disk. It
// stores a string representation of the time of the last scheduled flush.
const char kScheduleToFlushToDisk[] = "schedule_to_flush_to_disk";
}  // namespace user_prefs
