// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol IncognitoReauthConsumer <NSObject>

// Notify consumer that the displayed items require authentication before they
// can be accessed. Used for biometric incognito tab authentication.
- (void)setItemsRequireAuthentication:(BOOL)requireAuthentication;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSUMER_H_
