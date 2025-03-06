// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

// Flags defined in this file should have a comment above them that either
// marks them as permanent flags, or specifies what lifecycle stage they're at.
//
// Permanent flags are those that we'll keep around indefinitely because
// they're useful for testing, debugging, etc. These should be commented with
//    // Permanent flag
//
// Standard flags progress through a lifecycle and are eliminated at the end of
// it. The comments above them should be one of the following:
//    // Not yet enabled by default.
//    // Enabled in M123. Remove in or after M126.
//
// Every release or so we should cleanup and delete flags which have been
// default-enabled for long enough, based on the removal milestone in their
// comment.

#if BUILDFLAG(IS_WIN)
// Permanent flag
BASE_FEATURE(kWebAuthUseNativeWinApi,
             "WebAuthenticationUseNativeWinApi",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Permanent flag
BASE_FEATURE(kWebAuthCableExtensionAnywhere,
             "WebAuthenticationCableExtensionAnywhere",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enabled in M132. Remove in or after M135 or when the comparison histograms
// are not needed anymore.
BASE_FEATURE(kWebAuthnAndroidUsePasskeyCache,
             "WebAuthenticationAndroidUsePasskeyCache",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForGoogle,
             "WebAuthenticationICloudKeychainForGoogle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForActiveWithDrive,
             "WebAuthenticationICloudKeychainForActiveWithDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnICloudKeychainForActiveWithoutDrive,
             "WebAuthenticationICloudKeychainForActiveWithoutDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForInactiveWithDrive,
             "WebAuthenticationICloudKeychainForInactiveWithDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnICloudKeychainForInactiveWithoutDrive,
             "WebAuthenticationICloudKeychainForInactiveWithoutDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M135. Remove in or after M138.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_FEATURE(kWebAuthnRetryU2FErrors,
             "WebAuthenticationRetryU2FErrors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Development flag. Must not be enabled by default once
// kWebAuthnEnclaveAuthenticator is enabled.
BASE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys,
             "WebAuthenticationUseInsecureSoftwareUnexportableKeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M126. Remove in or after M129.
BASE_FEATURE(kWebAuthnCredProtectWin10BugWorkaround,
             "WebAuthenticationCredProtectWin10BugWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Development flag. Must not be enabled by default.
BASE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay,
             "WebAuthnEnclaveAuthenticatorDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAmbientSignin,
             "WebAuthenticationAmbientSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthniCloudKeychainPrf,
             "WebAuthenticationiCloudKeychainPrf",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This is a deprecation flag. It is now enabled by default, but we want to
// disable it eventually.
BASE_FEATURE(kWebAuthnHybridLinking,
             "WebAuthenticationHybridLinking",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This is a deprecation flag. It is now enabled by default, but we want to
// disable it eventually.
// Must not be disabled until kWebAuthnHybridLinking is disabled by default.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAuthnPublishPrelinkingInfo,
             "WebAuthenticationPublishPrelinkingInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Update the "last used" timestamp for GPM passkeys when asserted.
BASE_FEATURE(kWebAuthnUpdateLastUsed,
             "WebAuthenticationUpdateLastUsed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kWebAuthnHelloSignal,
             "WebAuthenticationHelloSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M132. Remove in or after M135.
BASE_FEATURE(kWebAuthnSkipHybridConfigIfSystemSupported,
             "WebAuthenticationSkipHybridConfigIfSystemSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kDigitalCredentialsHybridLinking,
             "DigitalCredentialsHybridLinking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M135. Remove in or after M138.
BASE_FEATURE(kWebAuthnPasskeyUpgrade,
             "WebAuthenticationPasskeyUpgrade",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kWebAuthnEnclaveAttestation,
             "WebAuthenticationEnclaveAttestation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M134. Remove in or after M137.
BASE_FEATURE(kWebAuthnNewBfCacheHandling,
             "WebAuthenticationNewBfCacheHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M134. Remove in or after M137.
BASE_FEATURE(kWebAuthnNoAccountTimeout,
             "WebAuthenticationNoAccountTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M134. Remove in or after M137.
BASE_FEATURE(kSyncSecurityDomainBeforePINRenewal,
             "kWebAuthenticationSyncSecurityDomainBeforePINRenewal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnRemoteDesktopAllowedOriginsPolicy,
             "WebAuthenticationRemoteDesktopAllowedOriginsPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M135. Remove in or after M138.
BASE_FEATURE(kWebAuthnMicrosoftSoftwareUnexportableKeyProvider,
             "WebAuthenticationMicrosoftSoftwareUnexportableKeyProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnSignalApiHidePasskeys,
             "WebAuthenticationSignalApiHidePasskeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace device
