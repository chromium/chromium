// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_DELEGATE_H_

@class AddAccountSigninMediator;

@protocol AddAccountSigninMediatorDelegate

- (void)mediatorWantsToBeStopped:(AddAccountSigninMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_DELEGATE_H_
