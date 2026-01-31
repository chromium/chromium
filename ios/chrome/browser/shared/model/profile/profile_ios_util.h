// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_UTIL_H_

#import <string>

class ProfileIOS;

// Whether `profile` is the personal profile.
bool IsPersonalProfile(ProfileIOS* profile);
// Whether `profile_name` is the name of the personal profile.
bool IsPersonalProfile(std::string_view profile_name);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_UTIL_H_
