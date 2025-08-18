// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_TEST_UTILS_H_

@class FakeSystemIdentity;
@protocol GREYMatcher;

namespace signin {

// A matcher for the snackbar message, when the user is signed in with primary
// identity `identity`.
id<GREYMatcher> snackbarMessageMatcher(FakeSystemIdentity* identity);

// Assert the snackbar is not shown for identity.
void assertSnackbarNotShown(FakeSystemIdentity* identity);

// Assert the snackbar is shown for `identity`.
void assertSnackbarShownAndDismissItWithIdentity(FakeSystemIdentity* identity);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_TEST_UTILS_H_
