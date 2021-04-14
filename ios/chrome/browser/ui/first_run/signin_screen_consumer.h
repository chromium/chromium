// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>

// Handles sign-in screen UI updates.
@protocol SigninScreenConsumer <NSObject>

// Sets user image when there is a known account.
- (void)setUserImage:(UIImage*)userImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_CONSUMER_H_
