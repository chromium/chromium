// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/is_uvpaa.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/common/content_client.h"
#include "device/fido/features.h"

#if BUILDFLAG(IS_MAC)
#include "content/public/browser/content_browser_client.h"
#include "device/fido/mac/authenticator.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "device/fido/win/authenticator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace content {

#if BUILDFLAG(IS_MAC)
void IsUVPlatformAuthenticatorAvailable(
    BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback callback) {
  const absl::optional<device::fido::mac::AuthenticatorConfig> config =
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
          AuthenticatorEnvironmentImpl::GetInstance()->win_webauthn_api(),
          std::move(callback));
}

#elif BUILDFLAG(IS_CHROMEOS)
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
#endif

}  // namespace content
