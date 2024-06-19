// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for FakeAuthActivityViewController.
extern NSString* const kFakeAuthActivityViewIdentifier;
// Accessibility identifier for the add account button in
// FakeAuthActivityViewController. See
// `+[SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:]`.
extern NSString* const kFakeAuthAddAccountButtonIdentifier;
// Accessibility identifier for the cancel button in
// FakeAuthActivityViewController.
extern NSString* const kFakeAuthCancelButtonIdentifier;

// Accessibility identifier for FakeAccountDetailsViewController.
extern NSString* const kFakeAccountDetailsViewIdentifier;
// Accessibility identifier for FakeAccountDetailsViewController cancel button.
extern NSString* const kFakeAccountDetailsDoneButtonIdentifier;

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_H_
