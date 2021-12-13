// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_util.h"

#include "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/signin/signin_util_internal.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSArray* GetScopeArray(const std::set<std::string>& scopes) {
  NSMutableArray* scopes_array = [[NSMutableArray alloc] init];
  for (const auto& scope : scopes) {
    [scopes_array addObject:base::SysUTF8ToNSString(scope)];
  }
  return scopes_array;
}

bool ShouldHandleSigninError(NSError* error) {
  return ios::provider::GetSigninErrorCategory(error) !=
         ios::provider::SigninErrorCategory::kUserCancellationError;
}

CGSize GetSizeForIdentityAvatarSize(IdentityAvatarSize avatar_size) {
  CGFloat size = 0;
  switch (avatar_size) {
    case IdentityAvatarSize::TableViewIcon:
      size = 30.;
      break;
    case IdentityAvatarSize::SmallSize:
      size = 32.;
      break;
    case IdentityAvatarSize::DefaultLarge:
      size = 40.;
      break;
  }
  DCHECK_NE(size, 0);
  return CGSizeMake(size, size);
}

signin::Tribool IsFirstSessionAfterDeviceRestore() {
  static signin::Tribool is_first_session_after_device_restore =
      signin::Tribool::kUnknown;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    is_first_session_after_device_restore =
        IsFirstSessionAfterDeviceRestoreInternal();
  });
  return is_first_session_after_device_restore;
}
