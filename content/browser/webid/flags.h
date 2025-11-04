// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

#include <optional>

// Flags to control WebID for testing/debugging.

namespace content::webid {

// Whether metrics endpoint is enabled.
bool IsMetricsEndpointEnabled();

// Whether the Delegation API is enabled.
bool IsDelegationEnabled();

// Whether the IdP Registration API is enabled.
bool IsIdPRegistrationEnabled();

// Whether the well-known enforcement is bypassed.
bool IsWithoutWellKnownEnforcementEnabled();

// Whether the Web Identity Digital Credentials API is enabled.
bool IsDigitalCredentialsEnabled();

// Whether the Web Identity Digital Credentials Creation API is enabled.
bool IsDigitalCredentialsCreationEnabled();

// Whether sending of SameSite=Lax cookies is enabled.
bool IsSameSiteLaxEnabled();

// Whether lightweight FedCM credentials are enabled.
bool IsLightweightModeEnabled();

// Whether phone/username is supported and name/email are optional.
bool IsAlternativeIdentifiersEnabled();

// Whether to support the newer syntax for the "Use Other Account"
// and account labels features.
bool IsUseOtherAccountAndLabelsNewSyntaxEnabled();

// Whether autofill enhancement with FedCM is enabled.
bool IsAutofillEnabled();

// Whether showing the iframe origin is enabled.
bool IsIframeOriginEnabled();

// Whether nonce usage in params is enabled.
bool IsNonceInParamsEnabled();

// Whether showing the non-string token is enabled.
bool IsNonStringTokenEnabled();

// Controls whether FedCM requires explicit accounts_endpoint and
// login_url in well-known files when using client_metadata.
bool IsWellKnownEndpointValidationEnabled();

// Whether preserving ports for testing is enabled.
bool IsPreservePortsForTestingEnabled();

// Whether accessing error attribute is enabled.
bool IsErrorAttributeEnabled();

// Whether the check for an embedder disabling auto sign-in is enabled.
bool IsFedCmEmbedderCheckEnabled();

// Whether navigation interception is enabled.
bool IsNavigationInterceptionEnabled();

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
