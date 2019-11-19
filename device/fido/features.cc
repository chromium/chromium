// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
const base::Feature kWebAuthUseNativeWinApi{"WebAuthenticationUseNativeWinApi",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

extern const base::Feature kWebAuthBiometricEnrollment{
    "WebAuthenticationBiometricEnrollment", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kWebAuthPhoneSupport{
    "WebAuthenticationPhoneSupport", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace device
