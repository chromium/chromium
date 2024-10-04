// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/containers/enum_set.h"

typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);

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

// Enum is used to represent the action to be taken by the authentication once
// the user is successfully signed in.
enum class PostSignInAction {
  // No post action after sign-in.
  kNone,
  kFirstType = kNone,
  // Shows a snackbar displaying the account that just signed-in.
  kShowSnackbar,
  // Enables SelectableType::kBookmarks for the account that just signed-in from
  // the bookmarks manager.
  kEnableUserSelectableTypeBookmarks,
  // Enables SelectableType::kReadingList for the account that just signed-in
  // from the reading list manager.
  kEnableUserSelectableTypeReadingList,
  kLastType = kEnableUserSelectableTypeReadingList
};

using PostSignInActionSet = base::EnumSet<PostSignInAction,
                                          PostSignInAction::kFirstType,
                                          PostSignInAction::kLastType>;

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
using SigninCompletionCallback = void (^)(SigninCoordinatorResult success);

// Completion callback for a sign-out operation.
// `success` is YES if the operation was successful.
using SignoutCompletionCallback = void (^)(BOOL success);

}  // namespace signin_ui

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CONSTANTS_H_
