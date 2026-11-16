// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_EMBEDDED_REAUTH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_EMBEDDED_REAUTH_COORDINATOR_H_

#import "components/webauthn/ios/ios_passkey_client.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the Passkey Embedded ReAuth flow.
@interface PasskeyEmbeddedReAuthCoordinator : ChromeCoordinator

// Designated initializer. `packagedLookupToken` is the challenge token for
// Embedded ReAuth. `callback` is executed with the ReAuth proof token on
// success, or an empty string on failure or dismissal.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
           packagedLookupToken:(NSString*)packagedLookupToken
                      callback:
                          (webauthn::IOSPasskeyClient::EmbeddedReAuthCallback)
                              callback NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_EMBEDDED_REAUTH_COORDINATOR_H_
