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

// WebAuthn spec error types returned to the relying party.
enum class WebAuthnError {
  kNotAllowedError,
  kInvalidStateError,
};

// "The authenticator used in the ceremony recognized an entry in
// excludeCredentials after the user consented to registering a credential."
// See: https://www.w3.org/TR/webauthn-3/#sctn-create-request-exceptions
inline constexpr char kInvalidStateErrorName[] = "InvalidStateError";
inline constexpr char kCredentialExcludedErrorMessage[] =
    "The user attempted to register an authenticator that contains one of the "
    "credentials already registered with the relying party.";

// "A catch-all error covering a wide range of possible reasons, including
// common ones like the user canceling out of the ceremony. Some of these causes
// are documented throughout this spec, while others are client-specific."
// See: https://www.w3.org/TR/webauthn-3/#sctn-create-request-exceptions
inline constexpr char kNotAllowedErrorName[] = "NotAllowedError";
inline constexpr char kNotAllowedErrorMessage[] =
    "The operation is not allowed.";

// Represents the code of an error returned when the user dismisses the GPM Pin
// flow by clicking the "Cancel" button.
// TODO(crbug.com/530911220): Define an enum and parse it in keychain provider.
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

// User verification statuses for passkey creation/assertion requests.
enum class PasskeyUserVerificationStatus {
  kNotRequired,
  kRequired,
  kCompleted,
};

// Helper types representing a key and a list of key respectively.
using SharedKey = std::vector<uint8_t>;
using SharedKeyList = std::vector<SharedKey>;

// Callback to be called once keys are fetched.
using KeysFetchedCallback = base::OnceCallback<void(SharedKeyList, NSError*)>;

// Callback to be called once keys are fetched, including user verification
// completion status.
using FetchKeysCallback =
    base::OnceCallback<void(SharedKeyList, bool did_complete_uv)>;

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_
