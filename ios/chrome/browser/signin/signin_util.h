// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#import <Foundation/Foundation.h>

#include <set>
#include <string>

@class ChromeIdentity;

// Returns an NSArray of |scopes| as NSStrings.
NSArray* GetScopeArray(const std::set<std::string>& scopes);

// Returns whether the given signin |error| should be handled.
//
// Note that cancel errors and errors handled internally by the signin component
// should not be handled.
bool ShouldHandleSigninError(NSError* error);

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
