// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_

#import <vector>

#import "base/functional/callback_forward.h"

// Type definitions generally useful for passkey requests.
namespace webauthn {

// The client-defined purpose of the reauthentication flow.
enum class ReauthenticatePurpose {
  // Unspecified action.
  kUnspecified,
  // The client is trying to encrypt using the shared key.
  kEncrypt,
  // The user is trying to decrypt using the shared key.
  kDecrypt,
};

// Helper types representing a key and a list of key respectively.
using SharedKey = std::vector<uint8_t>;
using SharedKeyList = std::vector<SharedKey>;

// Callback to be called once keys are fetched.
using KeysFetchedCallback = base::OnceCallback<void(const SharedKeyList&)>;

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TYPES_H_
