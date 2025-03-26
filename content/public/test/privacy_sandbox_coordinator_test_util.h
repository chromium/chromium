// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRIVACY_SANDBOX_COORDINATOR_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRIVACY_SANDBOX_COORDINATOR_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "content/public/browser/interest_group_manager.h"
#include "url/origin.h"

namespace content {

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
constexpr inline auto kTestPrivacySandboxCoordinatorPrivateKey =
    std::to_array<uint8_t>({
        0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
        0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
        0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
    });

constexpr inline auto kTestPrivacySandboxCoordinatorPublicKey =
    std::to_array<uint8_t>({
        0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
        0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
        0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
    });

// A fixed ID string and the corresponding ID. Used by
// CreatePrivacySandboxCoordinatorSerializedPublicKeys().
constexpr inline std::string_view kTestPrivacySandboxCoordinatorIdString =
    "12: Only the first two characters matter.";
constexpr std::uint8_t kTestPrivacySandboxCoordinatorId = 0x12;

// Creates a version 2 JSON privacy sandbox coordinator configuration that uses
// kTestPrivacySandboxCoordinatorPublicKey and
// kTestPrivacySandboxCoordinatorIdString for `origins` using `coordinator`.
// This can then either be used as a simulated HTTP response or passed directly
// to the key fetched.
std::string CreateTestPrivacySandboxCoordinatorSerializedPublicKeys(
    const url::Origin& coordinator,
    base::span<const url::Origin> origins);

// Convenience wrapper that calls
// CreatePrivacySandboxCoordinatorSerializedPublicKeys() to create keys, and
// then configures `interest_group_manager` to use them, and waits for them to
// be applied. May only be called once per `coordinator` in a test, since it
// sets up a single mock response for the coordinator.
void ConfigureTestPrivacySandboxCoordinatorKeys(
    InterestGroupManager* interest_group_manager,
    InterestGroupManager::TrustedServerAPIType api_type,
    const url::Origin& coordinator,
    base::span<const url::Origin> origins);

// Returns kTestPrivacySandboxCoordinatorPrivateKey as a string, which is the
// format taken by quiche's decode methods.
std::string GetTestPrivacySandboxCoordinatorPrivateKey();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PRIVACY_SANDBOX_COORDINATOR_TEST_UTIL_H_
