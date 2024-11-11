// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

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

BASE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege,
             "WebAuthenticationGoogleCorpRemoteDesktopClientPrivilege",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enabled in M129. Remove in or after M132.
BASE_FEATURE(kWebAuthnAndroidCredMan,
             "WebAuthenticationAndroidCredMan",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enabled in M132. Remove in or after M135 (when removing kWebAuthnGpmPin).
BASE_FEATURE(kWebAuthnEnclaveAuthenticator,
             "WebAuthenticationEnclaveAuthenticator",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M132. Remove in or after M135.
const base::FeatureParam<bool> kWebAuthnGpmPin{
    &kWebAuthnEnclaveAuthenticator, kWebAuthnGpmPinFeatureParameterName,
    /*default_value=*/true};

// Development flag. Must not be enabled by default once
// kWebAuthnEnclaveAuthenticator is enabled.
BASE_FEATURE(kWebAuthnUseInsecureSoftwareUnexportableKeys,
             "WebAuthenticationUseInsecureSoftwareUnexportableKeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled in M126. Remove in or after M129.
BASE_FEATURE(kWebAuthnCredProtectWin10BugWorkaround,
             "WebAuthenticationCredProtectWin10BugWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M130. Remove in or after M133.
BASE_FEATURE(kWebAuthnICloudRecoveryKey,
             "WebAuthenticationICloudRecoveryKey",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M130. Remove in or after M133.
BASE_FEATURE(kWebAuthnRecoverFromICloudRecoveryKey,
             "WebAuthenticationRecoverFromICloudRecoveryKey",
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

// Update the "last used" timestamp for GPM passkeys when asserted.
BASE_FEATURE(kWebAuthnUpdateLastUsed,
             "WebAuthenticationUpdateLastUsed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default enabled in M132. Remove in or after M135.
BASE_FEATURE(kWebAuthnSecurityKeyAndQrCodeUiRefresh,
             "WebAuthenticationSecurityKeyAndQrCodeUiRefresh",
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

}  // namespace device
