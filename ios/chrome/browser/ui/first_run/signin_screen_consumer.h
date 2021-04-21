// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles sign-in screen UI updates.
@protocol SigninScreenConsumer <NSObject>

// Sets user image when there is a known account. Pass nil to reset to the
// default image.
- (void)setUserImage:(UIImage*)userImage;

// Sets the |userName| and its |email| of the selected identity.
- (void)setSelectedIdentityUserName:(NSString*)userName email:(NSString*)email;

// Hides the identity control.
- (void)hideIdentityButtonControl;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_
