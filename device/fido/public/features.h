// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_FEATURES_H_
#define DEVICE_FIDO_PUBLIC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Allows the passkey unlock error UI to be shown.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kPasskeyUnlockErrorUi);

enum class PasskeyUnlockErrorUiExperimentArm {
  kUnlock,
  kGet,
  kVerify,
};
COMPONENT_EXPORT(FIDO_PUBLIC)
extern const base::FeatureParam<PasskeyUnlockErrorUiExperimentArm>
    kPasskeyUnlockErrorUiExperimentArm;
#endif

#if BUILDFLAG(IS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(FIDO_PUBLIC) BASE_DECLARE_FEATURE(kWebAuthUseNativeWinApi);
#endif  // BUILDFLAG(IS_WIN)

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthCableExtensionAnywhere);

COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnActorCheck);

// These five feature flags control whether iCloud Keychain is the default
// mechanism for platform credential creation in different situations.
// "Active" means that the user is an active user of the profile authenticator,
// defined by having used it in the past 31 days. "Drive" means that the user
// is currently signed into iCloud Drive, which isn't iCloud Keychain
// (what's needed), but the closest approximation that we can detect.

COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForGoogle);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithoutDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithoutDrive);

// Use insecure software unexportable keys to authenticate to the enclave.
// For development purposes only.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys);

// Send enclave requests with 5 seconds delay. For development purposes only.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay);

// Enable non-autofill sign-in UI for conditional mediation.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnAmbientSignin);

// Enables publishing prelinking information on Android.
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnPublishPrelinkingInfo);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the WebAuthn Signal API for Windows Hello.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnHelloSignal);

// When enabled, skips configuring hybrid when Windows can do hybrid. Hybrid may
// still be delegated to Windows regardless of this flag.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnSkipHybridConfigIfSystemSupported);

// Enables linking of hybrid devices to Chrome, both pre-linking (i.e. through
// Sync) and through hybrid for digital credentials requests.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kDigitalCredentialsHybridLinking);

// Checks attestation from the enclave service.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAttestation);

// Enables rate limiting of immediate requests based on main frame's eTLD+1.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnImmediateRequestRateLimit);

// Parameter controlling the maximum number of immediate requests allowed per
// origin (eTLD+1) within the time window. This applies to the longer of the
// two rate limiters.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(int,
                           kWebAuthnImmediateRequestLongRateLimitMaxRequests);

// Parameter controlling the time window (in seconds) for the immediate request
// rate limit. This applies to the longer of the two rate limiters.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(int,
                           kWebAuthnImmediateRequestLongRateLimitWindowSeconds);

// Parameter controlling the maximum number of immediate requests allowed per
// origin (eTLD+1) within the time window. This applies to the shorter of the
// two rate limiters.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(int,
                           kWebAuthnImmediateRequestShortRateLimitMaxRequests);

// Parameter controlling the time window (in seconds) for the immediate request
// rate limit. This applies to the shorter of the two rate limiters.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(
    int,
    kWebAuthnImmediateRequestShortRateLimitWindowSeconds);

// Enables the immediate mediation for `navigator.credentials.get` requests.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnImmediateGet);

// Parameter controlling the duration (in milliseconds) for the immediate
// mediation timeout.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(int, kWebAuthnImmediateMediationTimeoutMilliseconds);

// Enables support for the WebAuthenticationRemoteDesktopAllowedOrigins
// enterprise policy for Isolated Web Apps.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

// Sends a PIN generation number to the enclave on a PIN wrapping request.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnSendPinGeneration);

// Controls setting the `create_new_vault` flag when refreshing a PIN. When
// enabled, the enclave will produce new Vault parameters to create a new Vault
// instead of replacing it.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnNewRefreshFlow);

// Enables to save keys from out of context ("opportunistic") retrieval.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnOpportunisticRetrieval);

// Enables the fix of the logic for handling identity change (the new logic
// might decide to not terminate the running state machine in some cases,
// whereas the previous logic was always terminating the state machine).
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(
    kWebAuthnDoNotAlwaysTerminateStateMachineDuringIdentityChange);

// Parameter controlling the time window (in seconds) for keeping the cached
// opportunistically retrieved key in case its Gaia Id doesn't match to primary
// signed-in account.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE_PARAM(
    int,
    kWebAuthnOpportunisticRetrievalTimeToKeepCachedKeySeconds);

// Enables the logic of refreshing the state of GPM Enclave Controller.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnEnableRefreshingStateOfGpmEnclaveController);

// Support CTAP2.2 hmac-secret-mc extension in make credential request.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnHmacSecretMcExtension);

// Prompt the user to set a new PIN when user verification is required to
// fulfill a GPM passkey operation but no system UV or GPM PIN is available.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnCreatePinWhenSystemUvDisabled);

#if BUILDFLAG(IS_WIN)
// Enables support for PRF on create on Windows.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnWinPrfOnCreate);
#endif  // BUILDFLAG(IS_WIN)

// Enable max priority mode in WebAuthn websocket connections to cloud services.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnSocketMaxPriorityMode);

// Enables appending Google account session index (authuser=[index]) to the
// GPM PIN reset URL.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnGpmPinResetUsesAccountIndex);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_FEATURES_H_
