// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_SIGNOUT_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_SIGNOUT_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer protocol for the Age Mismatch prompt.
@protocol AgeMismatchSignoutConsumer <NSObject>

// Sets the user's primary account details for the identity view.
- (void)setPrimaryIdentityName:(NSString*)name
                         email:(NSString*)email
                        avatar:(UIImage*)avatar
                       managed:(BOOL)managed;

// Sets whether to show the "Stay signed out" button.
// This must be called before the view is presented.
- (void)setShowStaySignedOutButton:(BOOL)show;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_SIGNOUT_CONSUMER_H_
