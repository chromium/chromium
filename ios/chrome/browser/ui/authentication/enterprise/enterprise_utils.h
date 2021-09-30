// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

#import <UIKit/UIKit.h>

// List of Enterprise restriction options.
typedef NS_OPTIONS(NSUInteger, EnterpriseSignInRestrictions) {
  kNoEnterpriseRestriction = 0,
  kEnterpriseForceSignIn = 1 << 0,
  kEnterpriseRestrictAccounts = 1 << 1,

};

// Returns YES if some account restrictions are set.
bool IsRestrictAccountsToPatternsEnabled();

// Returns YES if force signIn is set.
bool IsForceSignInEnabled();

// Returns current EnterpriseSignInRestrictions.
EnterpriseSignInRestrictions GetEnterpriseSignInRestrictions();

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
