// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Error domain for authentication error.
extern NSString* kAuthenticationErrorDomain;

// The key in the user info dictionary containing the GoogleServiceAuthError
// code.
extern NSString* kGoogleServiceAuthErrorState;

typedef enum {
  // The error is wrapping a GoogleServiceAuthError.
  GOOGLE_SERVICE_AUTH_ERROR = -200,
  NO_AUTHENTICATED_USER = -201,
  CLIENT_ID_MISMATCH = -203,
  AUTHENTICATION_FLOW_ERROR = -206,
  TIMED_OUT_FETCH_POLICY = -210,
} AuthenticationErrorCode;

typedef enum {
  SHOULD_CLEAR_DATA_USER_CHOICE,
  SHOULD_CLEAR_DATA_CLEAR_DATA,
  SHOULD_CLEAR_DATA_MERGE_DATA,
} ShouldClearData;

// Enum is used to represent the action to be taken by the authentication once
// the user is successfully signed in.
enum class PostSignInAction {
  // No post action after sign-in.
  kNone,
  // Shows a snackbar displaying the account that just signed-in.
  kShowSnackbar,
  // TODO(crbug.com/1462858): Turn on sync was deprecated. Delete this enum
  // after phase 2 launches on iOS. See ConsentLevel::kSync documentation for
  // details.
  // Starts sign-in flow for a sync consent.
  // The owner of `AuthenticationFlow` still needs to:
  //  * Record the sync dialog strings.
  //  * Grand the sync consent in AuthenticationService.
  //  * Record the first setup complete.
  // Related crbug.com/1254359.
  kCommitSync,
};

// Enum for identity avatar size. See GetSizeForIdentityAvatarSize() to convert
// the enum value to point.
enum class IdentityAvatarSize {
  TableViewIcon,  // 30 pt.
  SmallSize,      // 32 pt.
  Regular,        // 40 pt.
  Large,          // 48 pt.
};

namespace signin_ui {

// Completion callback for a sign-in operation.
// `success` is YES if the operation was successful.
typedef void (^CompletionCallback)(BOOL success);

}  // namespace signin_ui

#endif  // IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_
