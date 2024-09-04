// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_METRICS_H_
#define IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_METRICS_H_

class ProfileManagerIOS;

// Records the number of profiles, including the number of active profiles.
void LogNumberOfProfiles(ProfileManagerIOS* manager);

#endif  // IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_METRICS_H_
