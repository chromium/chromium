// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

// Delegate for the passkey creation bottom sheet.
@protocol PasskeyCreationBottomSheetConsumer

// Sets the username, email and relying party identifier for the current passkey
// request.
- (void)setUsername:(NSString*)username
              email:(NSString*)email
               rpId:(NSString*)rpId;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_CONSUMER_H_
