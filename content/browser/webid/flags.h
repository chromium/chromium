// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

#include <optional>

// Flags to control WebID for testing/debugging.

namespace content {

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Whether metrics endpoint is enabled.
bool IsFedCmMetricsEndpointEnabled();

// Whether the Delegation API is enabled.
bool IsFedCmDelegationEnabled();

// Whether the IdP Registration API is enabled.
bool IsFedCmIdPRegistrationEnabled();

// Whether the well-known enforcement is bypassed.
bool IsFedCmWithoutWellKnownEnforcementEnabled();

// Whether the Web Identity Digital Credentials API is enabled.
bool IsWebIdentityDigitalCredentialsEnabled();

// Whether the Web Identity Digital Credentials Creation API is enabled.
bool IsWebIdentityDigitalCredentialsCreationEnabled();

// Whether sending of SameSite=Lax cookies is enabled.
bool IsFedCmSameSiteLaxEnabled();

// Whether showing filtered accounts is enabled.
bool IsFedCmShowFilteredAccountsEnabled();

// Whether lightweight FedCM credentials are enabled.
bool IsFedCmLightweightModeEnabled();

// Whether phone/username is supported and name/email are optional.
bool IsFedCmAlternativeIdentifiersEnabled();

// Whether cooldown on ignore is enabled.
bool IsFedCmCooldownOnIgnoreEnabled();

// Whether to support the newer syntax for the "Use Other Account"
// and account labels features.
bool IsFedCmUseOtherAccountAndLabelsNewSyntaxEnabled();

// Whether autofill enhancement with FedCM is enabled.
bool IsFedCmAutofillEnabled();

// Whether showing the iframe origin is enabled.
bool IsFedCmIframeOriginEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
