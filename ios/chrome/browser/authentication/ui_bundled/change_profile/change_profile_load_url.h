// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LOAD_URL_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LOAD_URL_H_

#import "ios/chrome/app/change_profile_continuation.h"

class GURL;

// Returns a ChangeProfileContinuation that opens the provided URL.
// This URL usually comes from a tab with this URL in the previous profile. In
// this case, it’s the caller responsibility to close this tab before the
// profile switching.
ChangeProfileContinuation CreateChangeProfileOpensURLContinuation(GURL url);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LOAD_URL_H_
