// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

// Flags to control WebID for testing/debugging.

namespace content {

// IDP IdpSigninStatus API modes.
enum class FedCmIdpSigninStatusMode { DISABLED, METRICS_ONLY, ENABLED };

// Whether FedCM auto sign-in is enabled.
bool IsFedCmAutoSigninEnabled();

// Whether FedCM IDP sign-out is enabled.
bool IsFedCmIdpSignoutEnabled();

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Returns the IdpSigninStatus API mode.
FedCmIdpSigninStatusMode GetFedCmIdpSigninStatusMode();

// Whether metrics endpoint is enabled.
bool IsFedCmMetricsEndpointEnabled();

// Whether the UserInfo API is enabled.
bool IsFedCmUserInfoEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
