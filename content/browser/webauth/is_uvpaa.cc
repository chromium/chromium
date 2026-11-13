// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/is_uvpaa.h"

#include <utility>

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <optional>

#include "base/feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/common/content_client.h"
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

void IsUVPlatformAuthenticatorAvailable(
    [[maybe_unused]] BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback callback) {
#if BUILDFLAG(IS_MAC)
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
#elif BUILDFLAG(IS_WIN)
  device::WinWebAuthnApiAuthenticator::
      IsUserVerifyingPlatformAuthenticatorAvailable(
          device::WinWebAuthnApi::GetDefault(), std::move(callback));
#elif BUILDFLAG(IS_CHROMEOS)
  device::ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
      std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

}  // namespace content
