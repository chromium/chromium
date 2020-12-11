// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/is_uvpaa.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/fido/features.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if defined(OS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace content {

#if defined(OS_MAC)
bool IsUVPlatformAuthenticatorAvailable(
    const content::AuthenticatorRequestClientDelegate::
        TouchIdAuthenticatorConfig& config) {
  return device::fido::mac::TouchIdAuthenticator::IsAvailable(config);
}

#elif defined(OS_WIN)
bool IsUVPlatformAuthenticatorAvailable(
    device::WinWebAuthnApi* win_webauthn_api) {
  return base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
         device::WinWebAuthnApiAuthenticator::
             IsUserVerifyingPlatformAuthenticatorAvailable(win_webauthn_api);
}

#elif defined(OS_CHROMEOS)
bool IsUVPlatformAuthenticatorAvailable() {
  return base::FeatureList::IsEnabled(
             device::kWebAuthCrosPlatformAuthenticator) &&
         device::ChromeOSAuthenticator::
             IsUVPlatformAuthenticatorAvailableBlocking();
}

#else
bool IsUVPlatformAuthenticatorAvailable() {
  return false;
}
#endif

}  // namespace content
