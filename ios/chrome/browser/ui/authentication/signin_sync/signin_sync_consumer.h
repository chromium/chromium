// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles sign-in screen UI updates.
@protocol SigninSyncConsumer <NSObject>

// Sets the `userName`, `email`, `givenName` and `avatar` of the selected
// identity. The `userName` and `givenName` can be nil. Notifies the UI that an
// identity is available.
- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar;

// Notifies the consumer that no identity is available and that the UI should be
// updated accordingly.
- (void)noIdentityAvailable;

// Sets the UI as interactable or not.
- (void)setUIEnabled:(BOOL)UIEnabled;

// Sets the value of the sign-in or sign out action to done (YES).
- (void)setActionToDone;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_CONSUMER_H_
