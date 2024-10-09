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

// Enable handling the passkeys reset flow.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPasskeysReset);

#if BUILDFLAG(IS_CHROMEOS)
// Enable ChromeOS native passkey support.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kChromeOsPasskeys);
#endif

// Support cross-domain RP ID assertions.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRelatedOrigin);

// Enable the Chrome Android cable authenticator. This lets a Chrome module
// handle cable requests from scanning a QR code, tapping on an FCM
// notification, or coming from Play Services. The Chrome Android cable
// authenticator has been replaced by an implementation in GMSCore, and this
// flag is here to help us safely remove the code.
//
// Note that the USB cable authenticator is not controlled by this flag. That
// feature hasn't shipped in GMSCore, so it is desirable to keep it around for a
// while longer.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnableAndroidCableAuthenticator);

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

// Cache responses from the security domain. To be used if we're overloading the
// security domain service.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnCacheSecurityDomain);

// Send enclave requests with 5 seconds delay. For development purposes only.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticatorDelay);

// Enable non-autofill sign-in UI for conditional mediation.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAmbientSignin);

// Support the PRF extension with iCloud Keychain credentials.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthniCloudKeychainPrf);

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
