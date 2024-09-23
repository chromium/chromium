// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_

#import <UIKit/UIKit.h>

// Sign-in status.
typedef NS_ENUM(NSUInteger, SigninScreenConsumerSigninStatus) {
  // Sign-in is available.
  SigninScreenConsumerSigninStatusAvailable,
  // Sign-in is forced.
  SigninScreenConsumerSigninStatusForced,
  // Sign-in is disabled.
  SigninScreenConsumerSigninStatusDisabled,
};

// List of sign-in screens.
typedef NS_ENUM(NSUInteger, SigninScreenConsumerScreenIntent) {
  // Show sign-in only.
  SigninScreenConsumerScreenIntentSigninOnly,
  // Show sign-in with welcome screen with terms of service and metric
  // reporting.
  SigninScreenConsumerScreenIntentWelcomeAndSignin,
  // Show sign-in with welcome screen with terms of service but without metric
  // reporting.
  SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin,
};

// Handles sign-in screen UI updates.
@protocol SigninScreenConsumer <NSObject>

// Shows details (an icon and a footer) that Chrome is managed by platform
// policies. This property needs to be set before the view is loaded.
@property(nonatomic, assign) BOOL hasPlatformPolicies;
// Sets if the screen intent see SigninScreenConsumerScreenIntent.
// This property needs to be set before the view is loaded.
@property(nonatomic, assign) SigninScreenConsumerScreenIntent screenIntent;
// Sets the sign-in status, see SigninScreenConsumerSigninStatus.
// This property needs to be set before the view is loaded.
@property(nonatomic, assign) SigninScreenConsumerSigninStatus signinStatus;
// In case the general sync-related UI is disabled: Shows a subtitle with
// benefits related to sync if the value is YES, a generic one otherwise.
@property(nonatomic, assign) BOOL syncEnabled;

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

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_CONSUMER_H_
