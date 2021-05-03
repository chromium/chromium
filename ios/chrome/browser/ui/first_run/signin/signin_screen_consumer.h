// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles sign-in screen UI updates.
@protocol SigninScreenConsumer <NSObject>

// Sets user image when there is a known account. Pass nil to reset to the
// default image.
- (void)setUserImage:(UIImage*)userImage;

// Sets the |userName| and its |email| of the selected identity. Notifies the UI
// that an identity is available.
- (void)setSelectedIdentityUserName:(NSString*)userName email:(NSString*)email;

// Notifies the consumer that no identity is available and that the UI should be
// updated accordingly.
- (void)noIdentityAvailable;

// Sets the UI as interactable or not.
- (void)setUIEnabled:(BOOL)UIEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_
