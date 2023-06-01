// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace device {

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWebAuthUseNativeWinApi,
             "WebAuthenticationUseNativeWinApi",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

BASE_FEATURE(kWebAuthCableExtensionAnywhere,
             "WebAuthenticationCableExtensionAnywhere",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kWebAuthCrosPlatformAuthenticator,
             "WebAuthenticationCrosPlatformAuthenticator",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAndroidHybridClientUi,
             "WebAuthenticationAndroidHybridClientUi",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege,
             "WebAuthenticationGoogleCorpRemoteDesktopClientPrivilege",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M114. Not yet enabled by default.
BASE_FEATURE(kWebAuthnAndroidCredMan,
             "WebAuthenticationAndroidCredMan",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Added in M115. Remove in or after M118.
BASE_FEATURE(kWebAuthnPinRequiredMeansNotRecognized,
             "WebAuthenticationPinRequiredMeansNotRecognized",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M115. Remove in or after M118
BASE_FEATURE(kWebAuthnHybridLinkWithoutNotifications,
             "WebAuthenticationHybridLinkWithoutNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnNoNullInJSON,
             "WebAuthenticationNoNullInJSON",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnRequireEasyAccessorFieldsInJSON,
             "WebAuthenticationRequireEasyAccessorFieldsInJSON",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Added in M115. Not yet enabled by default.
BASE_FEATURE(kWebAuthnICloudKeychain,
             "WebAuthenticationICloudKeychain",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace device
