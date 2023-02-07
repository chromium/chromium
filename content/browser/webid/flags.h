// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

// Flags to control WebID for testing/debugging.

namespace content {

// IDP IdpSigninStatus API modes.
enum class FedCmIdpSigninStatusMode { DISABLED, METRICS_ONLY, ENABLED };

// Whether FedCM auto re-authentication is enabled.
bool IsFedCmAutoReauthnEnabled();

// Whether FedCM IDP sign-out is enabled.
bool IsFedCmIdpSignoutEnabled();

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Returns the IdpSigninStatus API mode.
FedCmIdpSigninStatusMode GetFedCmIdpSigninStatusMode();

// Whether metrics endpoint is enabled.
bool IsFedCmMetricsEndpointEnabled();

// Whether the Relying Party Context API is enabled.
bool IsFedCmRpContextEnabled();

// Whether the UserInfo API is enabled.
bool IsFedCmUserInfoEnabled();

// Whether the Selective Disclosure API is enabled.
bool IsFedCmSelectiveDisclosureEnabled();

// Whether the login hint parameter is enabled.
bool IsFedCmLoginHintEnabled();

// Whether the IdP Registration API is enabled.
bool IsFedCmIdPRegistrationEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
