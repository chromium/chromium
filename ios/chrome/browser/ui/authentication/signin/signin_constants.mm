// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSkipSigninAccessibilityIdentifier =
    @"SkipSigninAccessibilityIdentifier";
NSString* const kAddAccountAccessibilityIdentifier =
    @"AddAccountAccessibilityIdentifier";
NSString* const kConfirmationAccessibilityIdentifier =
    @"ConfirmationAccessibilityIdentifier";
NSString* const kMoreAccessibilityIdentifier = @"MoreAccessibilityIdentifier";
NSString* const kWebSigninAccessibilityIdentifier =
    @"WebSigninAccessibilityIdentifier";
NSString* const kWebSigninPrimaryButtonAccessibilityIdentifier =
    @"WebSigninPrimaryButtonAccessibilityIdentifier";
NSString* const kWebSigninSkipButtonAccessibilityIdentifier =
    @"WebSigninSkipButtonAccessibilityIdentifier";
NSString* const kTangibleSyncViewAccessibilityIdentifier =
    @"TangibleSyncViewAccessibilityIdentifier";

const char* kWebSigninConsistencyConsecutiveActiveDismissalLimitParam =
    "consecutive_active_dismissal_limit";

const int kDefaultWebSignInDismissalCount = 3;
