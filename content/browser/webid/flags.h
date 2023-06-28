// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

// Flags to control WebID for testing/debugging.

namespace content {

// IDP IdpSigninStatus API modes.
enum class FedCmIdpSigninStatusMode { DISABLED, METRICS_ONLY, ENABLED };

// Whether the AuthZ is enabled or not.
bool IsFedCmAuthzEnabled();

// Whether FedCM IDP sign-out is enabled.
bool IsFedCmIdpSignoutEnabled();

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

// Whether the IdP Registration API is enabled.
bool IsFedCmIdPRegistrationEnabled();

// Whether the well-known enforcement is bypassed.
bool IsFedCmWithoutWellKnownEnforcementEnabled();

// Whether the Web Identity MDocs API is enabled.
bool IsWebIdentityMDocsEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
