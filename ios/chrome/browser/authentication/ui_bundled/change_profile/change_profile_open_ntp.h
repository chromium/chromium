// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_OPEN_NTP_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_OPEN_NTP_H_

#import "ios/chrome/app/change_profile_continuation.h"

// Returns a ChangeProfileContinuation that opens a New Tab page, unless the
// current tab (in the new profile) is already an NTP.
ChangeProfileContinuation CreateChangeProfileOpensNTPContinuation();

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_OPEN_NTP_H_
