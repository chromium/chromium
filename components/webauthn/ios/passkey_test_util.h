// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TEST_UTIL_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TEST_UTIL_H_

#import <string>
#import <string_view>
#import <vector>

#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/ios/passkey_request_params.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"

namespace webauthn {

inline constexpr char kRpId[] = "example.com";
inline constexpr char kFakeRequestId[] = "1effd8f52a067c8d3a01762d3c41dfd9";

// Converts an std::string_view to a uint8_t vector.
std::vector<uint8_t> AsByteVector(std::string_view str);

// Creates a test passkey using the default rp id.
sync_pb::WebauthnCredentialSpecifics GetTestPasskey(
    const std::string& credential_id);

// Builds PasskeyRequestParams using the default rp id.
PasskeyRequestParams BuildPasskeyRequestParams(
    device::UserVerificationRequirement user_verification,
    std::string request_id = kFakeRequestId,
    std::string frame_id = web::kMainFakeFrameId);

// Builds RegistrationRequestParams from an exclude credentials list.
RegistrationRequestParams BuildRegistrationRequestParams(
    const std::vector<device::PublicKeyCredentialDescriptor>&
        exclude_credentials,
    device::UserVerificationRequirement user_verification =
        device::UserVerificationRequirement::kPreferred,
    std::string request_id = kFakeRequestId,
    std::string frame_id = web::kMainFakeFrameId);

// Builds AssertionRequestParams from an allow credentials list.
AssertionRequestParams BuildAssertionRequestParams(
    const std::vector<device::PublicKeyCredentialDescriptor>& allow_credentials,
    device::UserVerificationRequirement user_verification =
        device::UserVerificationRequirement::kPreferred,
    std::string request_id = kFakeRequestId,
    std::string frame_id = web::kMainFakeFrameId);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TEST_UTIL_H_
