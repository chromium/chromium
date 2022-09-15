// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

// Flags to control WebID for testing/debugging.

namespace content {

// Whether FedCM auto sign-in is enabled.
bool IsFedCmAutoSigninEnabled();

// Whether FedCM IDP sign-out is enabled.
bool IsFedCmIdpSignoutEnabled();

// Whether manifest list fetching and validation is enabled.
bool IsFedCmManifestValidationEnabled();

// Whether multiple identity providers are enabled.
bool IsFedCmMultipleIdentityProvidersEnabled();

// Whether IdpSigninStatus is enabled.
bool IsFedCmIdpSigninStatusEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
