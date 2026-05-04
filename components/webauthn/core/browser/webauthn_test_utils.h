// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_TEST_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_TEST_UTILS_H_

#include <string_view>

#include "base/containers/span.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

// Utilities for testing implementations of the Authenticator mojo interface.

namespace webauthn::test {

struct OriginClaimedAuthorityPair {
  std::string_view origin;
  std::string_view claimed_authority;
  blink::mojom::AuthenticatorStatus expected_status;
};

// Returns a list of origin and relying party identifier test cases that should
// succeed.
base::span<const OriginClaimedAuthorityPair> GetValidRpTestCases();

// Returns a list of origin and relying party identifier test cases that should
// fail, and their expected error status.
base::span<const OriginClaimedAuthorityPair> GetInvalidRpTestCases();

}  // namespace webauthn::test

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_TEST_UTILS_H_
