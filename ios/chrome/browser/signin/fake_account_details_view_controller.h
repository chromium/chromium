// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_DETAILS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_DETAILS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol SystemIdentity;

// View controller used to fake "My Google Account" view in tests.
@interface FakeAccountDetailsViewController : UIViewController

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_DETAILS_VIEW_CONTROLLER_H_
