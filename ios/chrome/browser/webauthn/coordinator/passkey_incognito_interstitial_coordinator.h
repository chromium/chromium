// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_INCOGNITO_INTERSTITIAL_COORDINATOR_H_

#import "base/functional/callback.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the Passkey Incognito Interstitial.
@interface PasskeyIncognitoInterstitialCoordinator : ChromeCoordinator

// Handler used to dismiss the coordinator.
@property(nonatomic, weak) id<IOSPasskeyClientCommands> passkeyClientHandler;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  callback:
                                      (base::OnceCallback<void(bool)>)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
