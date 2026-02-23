// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace {

// Default maximum number of immediate requests allowed per origin (eTLD+1).
constexpr int kDefaultMaxRequests = 10;
// Default time window (in seconds) for the immediate request rate limit.
constexpr int kDefaultWindowSeconds = 60;
// Default timeout for immediate mediation requests (in milliseconds).
constexpr int kDefaultImmediateMediationTimeoutMs = 500;
// Default ttl (in seconds) for keeping the cached opportunistically retrieved
// key in case its Gaia Id doesn't match to primary signed-in account.
constexpr int kDefaultOpportunisticRetrievalTimeToKeepCachedKeySeconds = 300;

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Enabled in M146. Remove in or after M149.
BASE_FEATURE(kPasskeyUnlockManager, base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kPasskeyUnlockErrorUi, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<PasskeyUnlockErrorUiExperimentArm>::Option
    kPasskeyUnlockErrorUiExperimentArms[] = {
        {PasskeyUnlockErrorUiExperimentArm::kUnlock,
         "text_with_unlock_wording"},
        {PasskeyUnlockErrorUiExperimentArm::kGet, "text_with_get_wording"},
        {PasskeyUnlockErrorUiExperimentArm::kVerify,
         "text_with_verify_wording"},
};
constexpr base::FeatureParam<PasskeyUnlockErrorUiExperimentArm>
    kPasskeyUnlockErrorUiExperimentArm{
        &kPasskeyUnlockErrorUi, "passkey_unlock_ui_experiment_arm",
        PasskeyUnlockErrorUiExperimentArm::kVerify,
        &kPasskeyUnlockErrorUiExperimentArms};
#endif

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

// This is used to enable an experiment to reject WebAuthn requests
// when actor mode is on.
BASE_FEATURE(kWebAuthnActorCheck, base::FEATURE_ENABLED_BY_DEFAULT);

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

// Development flag. Must not be enabled by default once
// kWebAuthnEnclaveAuthenticator is enabled.
BASE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys,
             "WebAuthenticationUseInsecureSoftwareUnexportableKeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Development flag. Must not be enabled by default.
BASE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAmbientSignin,
             "WebAuthenticationAmbientSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Deprecation flag. Disabled by default in M145. Remove in or after M148.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAuthnPublishPrelinkingInfo,
             "WebAuthenticationPublishPrelinkingInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Disabled by default.
BASE_FEATURE(kWebAuthnHelloSignal,
             "WebAuthenticationHelloSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kDigitalCredentialsHybridLinking,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kWebAuthnEnclaveAttestation,
             "WebAuthenticationEnclaveAttestation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M144. Remove in or after M147.
BASE_FEATURE(kWebAuthnSignalApiHidePasskeys,
             "WebAuthenticationSignalApiHidePasskeys",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled by default as part of the WebAuthenticationImmediateGet feature. Do
// not remove before WebAuthenticationImmediateGet is removed.
BASE_FEATURE(kWebAuthnImmediateRequestRateLimit,
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

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnImmediateGet,
             "WebAuthenticationImmediateGet",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Deprecation flag. Disabled by default in M142. Remove in or after M145.
BASE_FEATURE(kWebAuthnSendPinGeneration,
             "WebAuthenticationSendPinGeneration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled by default in M145. Remove in or after M148.
BASE_FEATURE(kAuthenticatorPasswordsOnlyImmediateRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled by default in M146. Remove in or after M149.
BASE_FEATURE(kWebAuthnNewRefreshFlow,
             "WebAuthenticationNewRefreshFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled by default.
BASE_FEATURE(kWebAuthnOpportunisticRetrieval,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kWebAuthnOpportunisticRetrievalTimeToKeepCachedKeySeconds,
                   &kWebAuthnOpportunisticRetrieval,
                   "cached_key_ttl",
                   kDefaultOpportunisticRetrievalTimeToKeepCachedKeySeconds);

// Enabled by default in M143. Remove in or after M146.
BASE_FEATURE(kWebAuthenticationWindowsHints, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled by default in M144. Remove in or after M147.
BASE_FEATURE(kWebAuthnEnableRefreshingStateOfGpmEnclaveController,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled by default in M146. Remove in or after M149.
BASE_FEATURE(kWebAuthnHmacSecretMcExtension, base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kFedCmInAuthenticator, base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_FEATURE(kWebAuthnCreatePinWhenSystemUvDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enabled by default in M147. Remove in or after M150.
BASE_FEATURE(kWebAuthnWinPrfOnCreate, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
