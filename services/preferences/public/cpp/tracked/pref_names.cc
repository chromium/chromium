// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked/pref_names.h"

namespace user_prefs {

// A timestamp (stored in base::Time::ToInternalValue format) of the last time
// a preference was reset.
const char kPreferenceResetTime[] = "prefs.preference_reset_time";

}  // namespace user_prefs
