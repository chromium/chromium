// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LENS_SEARCH_CONTINUATION_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LENS_SEARCH_CONTINUATION_H_

#import "ios/chrome/app/change_profile_continuation.h"

enum class LensEntrypoint;

// Returns a ChangeProfileContinuation that starts a lens search.
ChangeProfileContinuation CreateChangeProfileLensSearchContinuation(
    LensEntrypoint entry_point);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_LENS_SEARCH_CONTINUATION_H_
