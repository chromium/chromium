// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_TEST_UTIL_H_

#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"

// A continuation that fails if it is executed.
ChangeProfileContinuation NotReachedContinuation();

// A continuation provider that fails if we try to access the continuation.
ChangeProfileContinuationProvider NotReachedContinuationProvider();

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_TEST_UTIL_H_
