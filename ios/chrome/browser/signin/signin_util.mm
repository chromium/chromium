// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_util.h"

#include "base/strings/sys_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"

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
  ios::SigninErrorProvider* provider =
      ios::GetChromeBrowserProvider().GetSigninErrorProvider();
  return ![provider->GetSigninErrorDomain() isEqualToString:error.domain] ||
         (error.code != provider->GetCode(ios::SigninError::CANCELED) &&
          error.code !=
              provider->GetCode(ios::SigninError::HANDLED_INTERNALLY));
}
