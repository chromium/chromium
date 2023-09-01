// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
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

#if BUILDFLAG(IS_CHROMEOS)
// Enable a ChromeOS platform authenticator
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthCrosPlatformAuthenticator);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Enable UI options to explicitly invoke hybrid CTAP authentication on
// Android.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidHybridClientUi);
#endif  // BUILDFLAG(IS_ANDROID)

// Feature flag for the Google-internal
// `WebAuthenticationAllowGoogleCorpRemoteRequestProxying` enterprise policy.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege);

#if BUILDFLAG(IS_ANDROID)
// Use the Android 14 Credential Manager API.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredMan);

// Use the Android 14 Credential Manager API for hybrid requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredManForHybrid);

// Use the Android 14 Credential Manager API alongside GMSCore.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredManAndGmsCore);
#endif  // BUILDFLAG(IS_ANDROID)

// Count kCtap2ErrPinRequired as meaning not recognised.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPinRequiredMeansNotRecognized);

// Advertise hybrid prelinking on Android even if the app doesn't have
// notifications permission.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHybridLinkWithoutNotifications);

// Don't allow the old style JSON where values could be `null`.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNoNullInJSON);

// Require the "easy accessor" fields to be provided in JSON attestation
// responses. Otherwise the fields are only checked if provided.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRequireEasyAccessorFieldsInJSON);

// Require up-to-date JSON formatting in remote-desktop contexts.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRequireUpToDateJSONForRemoteDesktop);

// Enable support for iCloud Keychain
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychain);

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

// Enable new hybrid UI
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNewHybridUI);

// Get caBLE pre-linking information from Play Services
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPrelinkPlayServices);

// Don't show the single-account sheet on macOS if Touch ID is available.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSkipSingleAccountMacOS);

// Delegate to Windows UI with webauthn.dll version six.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnWindowsUIv6);

// List synced GPM passkeys on webauthn credential pickers.
// Depends on kWebAuthnNewPasskeyUI.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnListSyncedPasskeys);

// Allow sites to opt into experimenting with conditional UI presentations.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthConditionalUIExperimentation);

// Handle caBLE requests on Android with the CredMan-capable code path.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnCableViaCredMan);

// Allow some sites to experiment with removing caBLE linking in requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnLinkingExperimentation);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// Enable use of a cloud enclave authenticator service.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticator);
#endif

// Serialize WebAuthn requests to JSON on the desktop. Useful for future
// projects but only concretely used for better logging at the time of writing.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnJSONSerializeRequests);

// Cache prelinking information on Android.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnCachePaaSK);

// Don't publish prelinking information if Chrome is running in a work profile
// on Android.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnDontPrelinkInProfiles);

// Use the new desktop passkey UI that has the following changes:
// * Display passkeys from multiple sources, including from Windows Hello,
//   alongside mechanisms on the modal UI.
// * Merge the QR and USB screens when available.
// * String tweaks on modal and conditional UI.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNewPasskeyUI);

// Sort discoverable credentials in the UI before showing.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnSortRecognizedCredentials);

// Don't configure discoveries like caBLE, iCloud Keychain, and the enclave,
// if the WebAuthn UI is disabled.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRequireUIForComplexDiscoveries);

// Filter a priori discovered credentials on google.com to those that have a
// user id that starts with "GOOGLE_ACCOUNT:".
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnFilterGooglePasskeys);

// Send the PIN protocol, if v2, in hmac-secret extensions.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPINProtocolInHMACSecret);

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
