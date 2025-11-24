// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Enables the Passkey Unlock Manager.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kPasskeyUnlockManager);

// Allows the passkey unlock error UI to be shown.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kPasskeyUnlockErrorUi);

enum class PasskeyUnlockErrorUiExperimentArm {
  kUnlock,
  kGet,
  kVerify,
};
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::FeatureParam<PasskeyUnlockErrorUiExperimentArm>
    kPasskeyUnlockErrorUiExperimentArm;
#endif

#if BUILDFLAG(IS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(DEVICE_FIDO) BASE_DECLARE_FEATURE(kWebAuthUseNativeWinApi);
#endif  // BUILDFLAG(IS_WIN)

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthCableExtensionAnywhere);

COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnActorCheck);

// These five feature flags control whether iCloud Keychain is the default
// mechanism for platform credential creation in different situations.
// "Active" means that the user is an active user of the profile authenticator,
// defined by having used it in the past 31 days. "Drive" means that the user
// is currently signed into iCloud Drive, which isn't iCloud Keychain
// (what's needed), but the closest approximation that we can detect.

COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForGoogle);
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithDrive);
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithoutDrive);
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithDrive);
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithoutDrive);

// Use insecure software unexportable keys to authenticate to the enclave.
// For development purposes only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys);

// Send enclave requests with 5 seconds delay. For development purposes only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay);

// Enable non-autofill sign-in UI for conditional mediation.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAmbientSignin);

// Enables publishing prelinking information on Android.
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPublishPrelinkingInfo);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the WebAuthn Signal API for Windows Hello.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHelloSignal);

#if BUILDFLAG(IS_ANDROID)
// Enables the WebAuthn Signal API for Chrome on Android.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidSignal);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, skips configuring hybrid when Windows can do hybrid. Hybrid may
// still be delegated to Windows regardless of this flag.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSkipHybridConfigIfSystemSupported);

// Enables linking of hybrid devices to Chrome, both pre-linking (i.e. through
// Sync) and through hybrid for digital credentials requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kDigitalCredentialsHybridLinking);

// Enable passkey upgrade requests in Google Password Manager.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPasskeyUpgrade);

// Checks attestation from the enclave service.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAttestation);

// Enables using the Microsoft Software Key Storage Provider to store
// unexportable keys when a TPM is not available.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnMicrosoftSoftwareUnexportableKeyProvider);

// Enables hiding passkeys instead of hard deleting them when reported as
// obsolete by the signal API.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSignalApiHidePasskeys);

// Enables rate limiting of immediate requests based on eTLD+1.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnImmediateRequestRateLimit);

// Parameter controlling the maximum number of immediate requests allowed per
// origin (eTLD+1) within the time window.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE_PARAM(int, kWebAuthnImmediateRequestRateLimitMaxRequests);

// Parameter controlling the time window (in seconds) for the immediate request
// rate limit.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE_PARAM(int,
                           kWebAuthnImmediateRequestRateLimitWindowSeconds);

// Enables the immediate mediation for `navigator.credentials.get` requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnImmediateGet);

// Parameter controlling the duration (in milliseconds) for the immediate
// mediation timeout.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE_PARAM(int, kWebAuthnImmediateMediationTimeoutMilliseconds);

// Enables autoselecting the single mechanism in immediate mediation requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnImmediateGetAutoselect);

// Enables large blob support for Google Password Manager.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnLargeBlobForGPM);

// Sends a PIN generation number to the enclave on a PIN wrapping request.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSendPinGeneration);

// Adds the cohort public key and cert.xml serial number to GPM wrapped PINs.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnWrapCohortData);

// Enables the Authenticator interface to support
// 'navigator.credentials.get({password: true, mediation: "immediate"})'
// requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kAuthenticatorPasswordsOnlyImmediateRequests);

// Controls setting the `create_new_vault` flag when refreshing a PIN. When
// enabled, the enclave will produce new Vault parameters to create a new Vault
// instead of replacing it.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNewRefreshFlow);

// If enabled, treats an empty enumeration of Windows Hello credentials the same
// as enumeration not being supported. This works around an issue where Windows
// Hello fails to enumerate credentials under RDP.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthenticationFixWindowsHelloRdp);

// When running an assertion operation, sends the enclave a hash of the client
// data JSON instead of the full contents.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthenticationHashClientDataJsonForEnclave);

// Enables to save keys from out of context ("opportunistic") retrieval.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnOpportunisticRetrieval);

// Enable support for WebAuthn hints through the Windows WebAuthn API.
// https://w3c.github.io/webauthn/#enum-hints.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthenticationWindowsHints);

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
