// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

#include <optional>

// Flags to control WebID for testing/debugging.

namespace content {

// Whether the authz flags has been overridden. If it has been overridden
// to false, we should not let it be enabled using an origin trial.
std::optional<bool> IsFedCmAuthzOverridden();

// Whether the AuthZ flag is enabled or not.
bool IsFedCmAuthzFlagEnabled();

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Whether metrics endpoint is enabled.
bool IsFedCmMetricsEndpointEnabled();

// Whether the Selective Disclosure API is enabled.
bool IsFedCmSelectiveDisclosureEnabled();

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

// Whether "Use Other Account" is enabled.
bool IsFedCmUseOtherAccountEnabled();

// Whether the ActiveMode feature is enabled.
bool IsFedCmActiveModeEnabled();

// Whether sending of SameSite=Lax cookies is enabled.
bool IsFedCmSameSiteLaxEnabled();

// Whether specifying a subset of the default fields is enabled.
bool IsFedCmFlexibleFieldsEnabled();

// Whether showing filtered accounts is enabled.
bool IsFedCmShowFilteredAccountsEnabled();

// Whether lightweight FedCM credentials are enabled.
bool IsFedCmLightweightModeEnabled();

// Whether phone/username is supported and name/email are optional.
bool IsFedCmAlternativeIdentifiersEnabled();

// Whether cooldown on ignore is enabled.
bool IsFedCmCooldownOnIgnoreEnabled();
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
