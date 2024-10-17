// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol IncognitoReauthConsumer <NSObject>

// Notify consumer that the displayed items require authentication before they
// can be accessed. Used for biometric incognito tab authentication.
// TODO(crbug.com/374073829): Remove after launching Soft Lock.
- (void)setItemsRequireAuthentication:(BOOL)requireAuthentication;

// Notify consumer that the displayed items require authentication before they
// can be accessed. Also push to the consumer the text and accessibility label
// of the primary button on the lock screen.
- (void)setItemsRequireAuthentication:(BOOL)requireAuthentication
                withPrimaryButtonText:(NSString*)text
                   accessibilityLabel:(NSString*)accessibilityLabel;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_
