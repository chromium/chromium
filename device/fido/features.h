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

#if BUILDFLAG(IS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(DEVICE_FIDO) BASE_DECLARE_FEATURE(kWebAuthUseNativeWinApi);
#endif  // BUILDFLAG(IS_WIN)

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthCableExtensionAnywhere);

#if BUILDFLAG(IS_ANDROID)
// Use the passkey cache service parallel to the FIDO2 module to retrieve
// passkeys from GMSCore. This is for comparison only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidUsePasskeyCache);
#endif  // BUILDFLAG(IS_ANDROID)

// These five feature flags control whether iCloud Keychain is the default
// mechanism for platform credential creation in different situations.
// "Active" means that the user is an active user of the profile authenticator,
// defined by having used it in the past 31 days. "Drive" means that the user
// is currently signed into iCloud Drive, which isn't iCloud Keychain
// (what's needed), but the cloest approximation that we can detect.

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

// Retry requests to U2F keys after a delay if a low-level error happens.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRetryU2FErrors);

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

// Enables linking of hybrid devices to Chrome, both pre-linking (i.e. through
// Sync) and through hybrid.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHybridLinking);

// Enables publishing prelinking information on Android.
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPublishPrelinkingInfo);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the WebAuthn Signal API for Windows Hello.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHelloSignal);

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

// With this flag, WebAuthn only disables the back-forward cache during the
// lifetime of a WebAuthn request.
// With the flag off, the back-forward cache is disabled for the lifetime of
// the page when a request is started.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNewBfCacheHandling);

// Removes the timeout when downloading the account state for the enclave,
// tweaking the UI:
// * A loading screen is shown when the enclave is selected.
// * The GPM error screen now has a "try another way" button.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNoAccountTimeout);

// When enabled, a sync with the Security Domain Service is performed before a
// GPM PIN renewal.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kSyncSecurityDomainBeforePINRenewal);

// Feature flag for the
// `WebAuthenticationRemoteDesktopAllowedOrigins` enterprise policy.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRemoteDesktopAllowedOriginsPolicy);

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

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
