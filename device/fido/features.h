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

// Use the Android 14 Credential Manager API for hybrid requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredManForHybrid);
#endif  // BUILDFLAG(IS_ANDROID)

// Advertise hybrid prelinking on Android even if the app doesn't have
// notifications permission.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnHybridLinkWithoutNotifications);

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

// Allow sites to opt into experimenting with conditional UI presentations.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthConditionalUIExperimentation);

// Allow some sites to experiment with removing caBLE linking in requests.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnLinkingExperimentation);

// Enable use of a cloud enclave authenticator service.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnEnclaveAuthenticator);

// Use the new desktop passkey UI that has the following changes:
// * Display passkeys from multiple sources, including from Windows Hello,
//   alongside mechanisms on the modal UI.
// * Merge the QR and USB screens when available.
// * String tweaks on modal and conditional UI.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnNewPasskeyUI);

// Filter a priori discovered credentials on google.com to those that have a
// user id that starts with "GOOGLE_ACCOUNT:".
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnFilterGooglePasskeys);

// Show an incognito confirmation sheet on Android when creating a credential.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidIncognitoConfirmation);

// Support evaluating PRFs during create() calls.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnPRFEvalDuringCreate);

#if BUILDFLAG(IS_CHROMEOS)
// Enable ChromeOS native passkey support.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kChromeOsPasskeys);
#endif

// A webauthn UI mode that detects screen readers and makes the dialog title
// focusable.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnScreenReaderMode);

// Update the minimum, maximum, and default timeout values for webauthn requests
// to be more generous and meet https://www.w3.org/TR/WCAG21/#enough-time.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAccessibleTimeouts);

// Support cross-domain RP ID assertions.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnRelatedOrigin);

// CHECK an invariant about credential sources.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnChromeImplementedInvariant);

// Allow extensions to assert WebAuthn relying party identifiers for domains
// they have host permissions for.
// Added in M121. Remove in or after M124.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kAllowExtensionsToSetWebAuthnRpIds);

// Send and receive JSON from Play Services.
COMPONENT_EXPORT(DEVICE_FIDO)
BASE_DECLARE_FEATURE(kWebAuthnAndroidFidoJson);

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
