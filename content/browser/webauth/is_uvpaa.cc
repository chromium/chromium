// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/is_uvpaa.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/features.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/fido/cros/authenticator.h"
#endif

namespace content {

#if defined(OS_MAC)
void IsUVPlatformAuthenticatorAvailable(
    const content::AuthenticatorRequestClientDelegate::
        TouchIdAuthenticatorConfig& config,
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  device::fido::mac::TouchIdAuthenticator::IsAvailable(config,
                                                       std::move(callback));
}

#elif defined(OS_WIN)
void IsUVPlatformAuthenticatorAvailable(
    device::WinWebAuthnApi* win_webauthn_api,
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  device::WinWebAuthnApiAuthenticator::
      IsUserVerifyingPlatformAuthenticatorAvailable(win_webauthn_api,
                                                    std::move(callback));
}

#elif BUILDFLAG(IS_CHROMEOS_ASH)
void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  if (!base::FeatureList::IsEnabled(
          device::kWebAuthCrosPlatformAuthenticator)) {
    std::move(callback).Run(false);
    return;
  }
  device::ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
      std::move(callback));
}

#else
void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  std::move(callback).Run(false);
}
#endif

}  // namespace content
