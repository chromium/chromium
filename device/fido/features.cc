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

BASE_FEATURE(kWebAuthnGoogleCorpRemoteDesktopClientPrivilege,
             "WebAuthenticationGoogleCorpRemoteDesktopClientPrivilege",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthPasskeysUI,
             "WebAuthenticationPasskeysUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthnNoEmptyDisplayNameCBOR,
             "WebAuthenticationNoEmptyDisplayNameCBOR",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthnNonDiscoverableMakeCredentialQRFlag,
             "WebAuthenticationNonDiscoverableMakeCredentialQRFlag",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDisableWebAuthnWithBrokenCerts,
             "DisableWebAuthnWithBrokenCerts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthnNoPasskeysError,
             "WebAuthenticationNoPasskeysError",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M112. Remove in or after M115.
BASE_FEATURE(kWebAuthnCredProtectThree,
             "WebAuthenticationCredProtectThree",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M112. Enabled in M113. Remove in or after M116.
BASE_FEATURE(kWebAuthnPRFAsAuthenticator,
             "WebAuthenticationPRFAsAuthenticator",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Added in M113. Remove in or after M116.
BASE_FEATURE(kWebAuthnMacPlatformAuthenticatorOptionalUv,
             "WebAuthenticationMacPlatformAuthenticatorOptionalUv",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthnPhoneConfirmationSheet,
             "WebAuthenticationPhoneConfirmationSheet",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Add in M113. Remove in or after M116.
BASE_FEATURE(kWebAuthnNewPrioritiesImpl,
             "WebAuthenticationNewPrioritiesImpl",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace device
