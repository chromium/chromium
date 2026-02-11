// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "base/functional/callback_forward.h"

// Type definitions generally useful for passkey requests.
namespace webauthn {

// Represents the code of an error returned when the user dismisses the GPM Pin
// flow by clicking the "Cancel" button.
// TODO(crbug.com/460485614): Define an enum and parse it in keychain provider.
static const NSInteger kErrorUserDismissedGPMPinFlow = -105;

// Block type used for the completion of the primary action button tap in
// passkey welcome screen, passing the navigation controller that displayed the
// screen.
typedef void (^PasskeyWelcomeScreenAction)(
    UINavigationController* navigationController);

// The client-defined purpose of the reauthentication flow.
enum class ReauthenticatePurpose {
  // Unspecified action.
  kUnspecified,
  // The client is trying to encrypt using the shared key.
  kEncrypt,
  // The user is trying to decrypt using the shared key.
  kDecrypt,
};

// Possible purposes for showing the passkey welcome screen.
enum class PasskeyWelcomeScreenPurpose {
  kEnroll,
  kFixDegradedRecoverability,
  kReauthenticate,
};

// Helper types representing a key and a list of key respectively.
using SharedKey = std::vector<uint8_t>;
using SharedKeyList = std::vector<SharedKey>;

// Callback to be called once keys are fetched.
using KeysFetchedCallback = base::OnceCallback<void(SharedKeyList, NSError*)>;

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_
