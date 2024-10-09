// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

#include <optional>

// Flags to control WebID for testing/debugging.

namespace content {

// IDP IdpSigninStatus API modes.
enum class FedCmIdpSigninStatusMode { METRICS_ONLY, ENABLED };

// Whether the authz flags has been overridden. If it has been overridden
// to false, we should not let it be enabled using an origin trial.
std::optional<bool> IsFedCmAuthzOverridden();

// Whether the AuthZ flag is enabled or not.
bool IsFedCmAuthzFlagEnabled();

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Returns the IdpSigninStatus API mode.
// Most callers should use webid::GetIdpSigninStatusMode() in webid_utils.h
// instead, as that version takes origin trial status into account.
FedCmIdpSigninStatusMode GetFedCmIdpSigninStatusFlag();

// Whether metrics endpoint is enabled.
bool IsFedCmMetricsEndpointEnabled();

// Whether the Selective Disclosure API is enabled.
bool IsFedCmSelectiveDisclosureEnabled();

// Whether we should only send SameSite=None cookies for credentialed requests.
// (only affects non-CORS requests, because CORS already only sends
// SameSite=None)
bool IsFedCmSameSiteNoneEnabled();

// Whether the IdP Registration API is enabled.
bool IsFedCmIdPRegistrationEnabled();

// Whether the well-known enforcement is bypassed.
bool IsFedCmWithoutWellKnownEnforcementEnabled();

// Whether the Web Identity Digital Credentials API is enabled.
bool IsWebIdentityDigitalCredentialsEnabled();

// Whether "Use Other Account" is enabled.
bool IsFedCmUseOtherAccountEnabled(bool is_active_mode);

// Whether the ActiveMode feature is enabled.
bool IsFedCmActiveModeEnabled();

// Whether sending of SameSite=Lax cookies is enabled.
bool IsFedCmSameSiteLaxEnabled();

// Whether specifying a subset of the default fields is enabled.
bool IsFedCmFlexibleFieldsEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
