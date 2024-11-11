// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace device {

#if BUILDFLAG(IS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(DEVICE_FIDO) BASE_DECLARE_FEATURE(kWebAuthUseNativeWinApi);
#endif  // BUILDFLAG(IS_WIN)

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthCableExtensionAnywhere);

// Feature flag for the Google-internal
// `WebAuthenticationAllowGoogleCorpRemoteRequestProxying` enterprise policy.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege);

#if BUILDFLAG(IS_ANDROID)
// Use the Android 14 Credential Manager API.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredMan);

// Use the Android 14 Credential Manager API for credentials stored in Gmscore.
COMPONENT_EXPORT(DEVICE_FIDO)
inline constexpr base::FeatureParam<bool> kWebAuthnAndroidGpmInCredMan{
    &kWebAuthnAndroidCredMan, "gpm_in_cred_man", false};

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

// Enable use of a cloud enclave authenticator service.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticator);

// Enable use of Google Password Manager PIN.
const char kWebAuthnGpmPinFeatureParameterName[] = "WebAuthenticationGpmPin";
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::FeatureParam<bool> kWebAuthnGpmPin;

// Use insecure software unexportable keys to authenticate to the enclave.
// For development purposes only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys);

// Enable a workaround for an interaction between Windows 10 and certain
// security keys.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnCredProtectWin10BugWorkaround);

// Store recovery keys on iCloud keychain for the enclave authenticator.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudRecoveryKey);

// Retrieve and recover from recovery keys on iCloud keychain for the enclave
// authenticator.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRecoverFromICloudRecoveryKey);

// Send enclave requests with 5 seconds delay. For development purposes only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay);

// Enable non-autofill sign-in UI for conditional mediation.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAmbientSignin);

// Support the PRF extension with iCloud Keychain credentials.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthniCloudKeychainPrf);

// Enables linking of hybrid devices to Chrome, both pre-linking (i.e. through
// Sync) and through hybrid.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHybridLinking);

// Update the "last_used" timestamp in GPM passkeys when asserted.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnUpdateLastUsed);

// Enables the refreshed UI for QR codes and security keys.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSecurityKeyAndQrCodeUiRefresh);

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

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
