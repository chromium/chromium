// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_prefs.h"

#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_types.h"

namespace default_status {

const char kDefaultStatusAPICohort[] = "DefaultStatusAPICohort";
const char kDefaultStatusAPILastSuccessfulCall[] =
    "DefaultStatusAPILastSuccessfulCall";
const char kDefaultStatusAPINextRetry[] = "DefaultStatusAPINextRetry";
const char kDefaultStatusAPIResult[] = "DefaultStatusAPIResult";

void RegisterDefaultStatusPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kDefaultStatusAPICohort, 0);
  registry->RegisterTimePref(kDefaultStatusAPILastSuccessfulCall, base::Time());
  registry->RegisterTimePref(kDefaultStatusAPINextRetry, base::Time());
  registry->RegisterIntegerPref(
      kDefaultStatusAPIResult,
      static_cast<int>(DefaultStatusAPIResult::kUnknown));
}

}  // namespace default_status
