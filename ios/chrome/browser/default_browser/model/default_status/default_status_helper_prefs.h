// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_PREFS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_PREFS_H_

class PrefRegistrySimple;

namespace default_status {

// The cohort this client belongs to for the purpose of reporting the result of
// the default status system API call.
extern const char kDefaultStatusAPICohort[];

// The timestsamp of the last successful call to the default status API call. A
// successful call is one that does not yield an error, regardless of the
// default status result.
extern const char kDefaultStatusAPILastSuccessfulCall[];

// The timestamp after which the default status API should be called again for
// this client.
extern const char kDefaultStatusAPINextRetry[];

// The result of the last successful default status API call. The value is of
// type DefaultStatusCheckResult and defaults to kUnknown.
extern const char kDefaultStatusAPIResult[];

// Registers the prefs associated with the default status helper.
void RegisterDefaultStatusPrefs(PrefRegistrySimple* registry);

}  // namespace default_status

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_PREFS_H_
