// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace {

// Default maximum number of immediate requests allowed per origin (eTLD+1).
constexpr int kDefaultMaxRequests = 10;
// Default time window (in seconds) for the immediate request rate limit.
constexpr int kDefaultWindowSeconds = 60;
// Default timeout for immediate mediation requests (in milliseconds).
constexpr int kDefaultImmediateMediationTimeoutMs = 500;

}  // namespace

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

// Development flag. Must not be enabled by default.
BASE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay,
             "WebAuthnEnclaveAuthenticatorDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAmbientSignin,
             "WebAuthenticationAmbientSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is a deprecation flag. Disabled in M136. Remove in or after M139.
BASE_FEATURE(kWebAuthnHybridLinking,
             "WebAuthenticationHybridLinking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is a deprecation flag. It is now enabled by default, but we want to
// disable it eventually.
// Must not be disabled until kWebAuthnHybridLinking is disabled by default.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAuthnPublishPrelinkingInfo,
             "WebAuthenticationPublishPrelinkingInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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

// Default enabled in M136. Remove in or after M139.
BASE_FEATURE(kWebAuthnPasskeyUpgrade,
             "WebAuthenticationPasskeyUpgrade",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

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

// Default enabled in M136. Remove in or after M139.
BASE_FEATURE(kWebAuthnRemoteDesktopAllowedOriginsPolicy,
             "WebAuthenticationRemoteDesktopAllowedOriginsPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M135. Remove in or after M138.
BASE_FEATURE(kWebAuthnMicrosoftSoftwareUnexportableKeyProvider,
             "WebAuthenticationMicrosoftSoftwareUnexportableKeyProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnSignalApiHidePasskeys,
             "WebAuthenticationSignalApiHidePasskeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled by default as part of the WebAuthenticationImmediateGet feature. Do
// not remove before WebAuthenticationImmediateGet is removed.
BASE_FEATURE(kWebAuthnImmediateRequestRateLimit,
             "WebAuthnImmediateRequestRateLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kWebAuthnImmediateRequestRateLimitMaxRequests,
                   &kWebAuthnImmediateRequestRateLimit,
                   "max_requests",
                   kDefaultMaxRequests);

BASE_FEATURE_PARAM(int,
                   kWebAuthnImmediateRequestRateLimitWindowSeconds,
                   &kWebAuthnImmediateRequestRateLimit,
                   "window_seconds",
                   kDefaultWindowSeconds);

// Enabled by default on Desktop for the Origin Trial. Do not remove until the
// Origin Trial expires.
BASE_FEATURE(kWebAuthnImmediateGet,
             "WebAuthenticationImmediateGet",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE_PARAM(int,
                   kWebAuthnImmediateMediationTimeoutMilliseconds,
                   &kWebAuthnImmediateGet,
                   "timeout_ms",
                   kDefaultImmediateMediationTimeoutMs);

// Enabled by default. Remove the flag and the logic (as if the flag is in
// disabled state) when the WebAuthenticationImmediateGet origin trial is over.
BASE_FEATURE(kWebAuthnImmediateGetAutoselect,
             "WebAuthenticationImmediateGetAutoselect",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace device
