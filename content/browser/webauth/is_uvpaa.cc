// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/is_uvpaa.h"

#include <optional>
#include <utility>

#include "build/build_config.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/common/content_client.h"

#if BUILDFLAG(IS_MAC)
#include "base/feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/icloud_keychain.h"
#include "device/fido/public/features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace content {

#if BUILDFLAG(IS_MAC)
void IsUVPlatformAuthenticatorAvailable(
    BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnICloudKeychainUseDeviceConfiguredForPasskeys) &&
      device::fido::icloud_keychain::IsSupported()) {
    if (__builtin_available(macOS 26.2, *)) {
      if (device::fido::icloud_keychain::IsConfiguredForPasskeys()) {
        std::move(callback).Run(true);
        return;
      }
    }
  }

  const std::optional<device::fido::mac::AuthenticatorConfig> config =
      GetContentClient()
          ->browser()
          ->GetWebAuthenticationDelegate()
          ->GetTouchIdAuthenticatorConfig(browser_context);
  if (!config) {
    std::move(callback).Run(false);
    return;
  }
  device::fido::mac::TouchIdAuthenticator::IsAvailable(*config,
                                                       std::move(callback));
}

#elif BUILDFLAG(IS_WIN)
void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  device::WinWebAuthnApiAuthenticator::
      IsUserVerifyingPlatformAuthenticatorAvailable(
          device::WinWebAuthnApi::GetDefault(), std::move(callback));
}

#elif BUILDFLAG(IS_CHROMEOS)
void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  device::ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
      std::move(callback));
}
#endif

}  // namespace content
