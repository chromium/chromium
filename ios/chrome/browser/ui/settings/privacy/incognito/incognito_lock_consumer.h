// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_CONSUMER_H_

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"

// Consumer for the IncognitoLockMediator to update the
// IncognitoLockViewController.
@protocol IncognitoLockConsumer <NSObject>

// Sets the incognito lock state on the consumer.
- (void)setIncognitoLockState:(IncognitoLockState)state;

@end
#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_CONSUMER_H_
