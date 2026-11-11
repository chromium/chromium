// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_TEST_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_TEST_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface to configure Remote Actor Keychain credentials and build valid
// Remote Actor deep-link URLs for EarlGrey tests.
@interface RemoteActorCredentialBottomSheetAppInterface : NSObject

// Generates a symmetric key in the Keychain (with Keychain access group
// override cleared so simulator queries succeed), encrypts a Remote Actor
// password upload request for `domain` and `gaiaID`, and returns the resulting
// Remote Actor URL string (or `nil` on failure).
+ (NSString*)createValidRemoteActorURLForDomain:(NSString*)domain
                                         gaiaID:(NSString*)gaiaID;

// Removes the test Remote Actor symmetric key from the Keychain and resets the
// Keychain access group override.
+ (void)cleanUpRemoteActorKeychain;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_TEST_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_APP_INTERFACE_H_
