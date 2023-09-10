// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include <vector>

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

#if BUILDFLAG(IS_CHROMEOS)
// Enabled in M102. Ready to be removed.
BASE_FEATURE(kWebAuthCrosPlatformAuthenticator,
             "WebAuthenticationCrosPlatformAuthenticator",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnAndroidHybridClientUi,
             "WebAuthenticationAndroidHybridClientUi",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege,
             "WebAuthenticationGoogleCorpRemoteDesktopClientPrivilege",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Added in M114. Not yet enabled by default.
BASE_FEATURE(kWebAuthnAndroidCredMan,
             "WebAuthenticationAndroidCredMan",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnAndroidCredManForHybrid,
             "WebAuthenticationAndroidCredManForHybrid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M118, Not yet enabled by default.
BASE_FEATURE(kWebAuthnAndroidCredManAndGmsCore,
             "WebAuthenticationAndroidCredManAndGmsCore",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Added in M115. Remove in or after M118.
BASE_FEATURE(kWebAuthnPinRequiredMeansNotRecognized,
             "WebAuthenticationPinRequiredMeansNotRecognized",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M115. Remove in or after M118
BASE_FEATURE(kWebAuthnHybridLinkWithoutNotifications,
             "WebAuthenticationHybridLinkWithoutNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnNoNullInJSON,
             "WebAuthenticationNoNullInJSON",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnRequireEasyAccessorFieldsInJSON,
             "WebAuthenticationRequireEasyAccessorFieldsInJSON",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnRequireUpToDateJSONForRemoteDesktop,
             "WebAuthenticationRequireUpToDateJSONForRemoteDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychain,
             "WebAuthenticationICloudKeychain",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnNewHybridUI,
             "WebAuthenticationNewHybridUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnPrelinkPlayServices,
             "WebAuthenticationPrelinkPlayServices",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnSkipSingleAccountMacOS,
             "WebAuthenticationSkipSingleAccountMacOS",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M116. Remove in or after M119.
BASE_FEATURE(kWebAuthnWindowsUIv6,
             "WebAuthenticationWindowsUIv6",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnListSyncedPasskeys,
             "WebAuthenticationListSyncedPasskeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthConditionalUIExperimentation,
             "WebAuthenticationConditionalUIExperimentation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnCableViaCredMan,
             "WebAuthenticationCableViaCredMan",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnLinkingExperimentation,
             "WebAuthenticationLinkingExperimentation",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// Not yet enabled by default.
BASE_FEATURE(kWebAuthnEnclaveAuthenticator,
             "WebAuthenticationEnclaveAuthenticator",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnJSONSerializeRequests,
             "WebAuthenticationJSONSerializeRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnCachePaaSK,
             "WebAuthenticationCachePaaSK",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M117. Remove in or after M120.
BASE_FEATURE(kWebAuthnDontPrelinkInProfiles,
             "WebAuthenticationDontPrelinkInProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118 for macOS only.
BASE_FEATURE(kWebAuthnNewPasskeyUI,
             "WebAuthenticationNewPasskeyUI",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnSortRecognizedCredentials,
             "WebAuthenticationSortRecognizedCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnRequireUIForComplexDiscoveries,
             "WebAuthenticationRequestUIForComplexDiscoveries",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118 for non-ChromeOS (see crbug.com/1480732).
BASE_FEATURE(kWebAuthnFilterGooglePasskeys,
             "WebAuthenticationFilterGooglePasskeys",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnPINProtocolInHMACSecret,
             "WebAuthenticationPINProtocolInHMACSecret",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace device
