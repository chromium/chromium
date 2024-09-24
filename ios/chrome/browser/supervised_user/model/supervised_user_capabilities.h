// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace supervised_user {

// Returns true if the profile is subjected to parental controls, based on
// AccountInfo capabilities or prefs.
bool IsSubjectToParentalControls(ProfileIOS* profile);

}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_H_
