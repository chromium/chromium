// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_COMMANDS_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_COMMANDS_H_

#import "components/webauthn/ios/ios_passkey_client.h"

// Commands related to the IOSPasskeyClient.
@protocol IOSPasskeyClientCommands

// Shows the passkey creation bottom sheet.
- (void)showPasskeyCreationBottomSheet:(const std::string&)requestID;

// Shows the passkey suggestion bottom sheet.
- (void)showPasskeySuggestionBottomSheet:
    (webauthn::IOSPasskeyClient::RequestInfo)requestInfo;

// Shows the passkey welcome screen for the given `purpose`.
- (void)showPasskeyWelcomeScreenForPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                                completion:
                                    (webauthn::PasskeyWelcomeScreenAction)
                                        completion;

// Dismisses the passkey welcome screen.
- (void)dismissPasskeyWelcomeScreen;

@end

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_COMMANDS_H_
