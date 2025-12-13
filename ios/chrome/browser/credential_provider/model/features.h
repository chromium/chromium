// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"

// Feature flag to enable passkey PRF support in the credential provider.
BASE_DECLARE_FEATURE(kCredentialProviderPasskeyPRF);

// Feature flag to enable passkey Large Blob support in the credential provider.
BASE_DECLARE_FEATURE(kCredentialProviderPasskeyLargeBlob);

// Feature flag to enable the performance improvements for the credential
// provider.
BASE_DECLARE_FEATURE(kCredentialProviderPerformanceImprovements);

// Feature flag to enable signal API in the credential provider.
BASE_DECLARE_FEATURE(kCredentialProviderSignalAPI);

// Returns whether the CPE Performance Improvement Feature is enabled.
bool IsCPEPerformanceImprovementsEnabled();

// Credential exchange feature is controlled by a build-time flag, because
// it is defined in capabilities that are linked to the Credential Provider
// Extension and it must be declared in its Info.plist (manifest).
constexpr bool CredentialExchangeEnabled() {
#if BUILDFLAG(IOS_CREDENTIAL_EXCHANGE_ENABLED)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IOS_PASSKEYS_ENABLED)
}

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_
