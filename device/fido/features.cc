// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "url/origin.h"

namespace device {

#if BUILDFLAG(IS_WIN)
const base::Feature kWebAuthUseNativeWinApi{"WebAuthenticationUseNativeWinApi",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN)

extern const base::Feature kWebAuthPhoneSupport{
    "WebAuthenticationPhoneSupport", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kWebAuthPasskeysUI{
    "WebAuthenticationPasskeysUI", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kWebAuthCableDisco{
    "WebAuthenticationCableDisco", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kWebAuthCableExtensionAnywhere{
    "WebAuthenticationCableExtensionAnywhere",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS)
const base::Feature kWebAuthCrosPlatformAuthenticator{
    "WebAuthenticationCrosPlatformAuthenticator",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS)

extern const base::Feature kU2fPermissionPrompt{
    "U2fPermissionPrompt", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kWebAuthnGoogleCorpRemoteDesktopClientPrivilege{
    "WebAuthenticationGoogleCorpRemoteDesktopClientPrivilege",
    base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kWebAuthPasskeysUIExperiment{
    "WebAuthenticationPasskeysUIExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace device
