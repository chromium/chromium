// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IS_UVPAA_H_
#define CONTENT_PUBLIC_BROWSER_IS_UVPAA_H_

#include "base/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_request_client_delegate.h"

#if defined(OS_WIN)
namespace device {
class WinWebAuthnApi;
}
#endif

namespace content {

// This provides the basic platform-specific implementations of
// IsUserVerifyingPlatformAuthenticatorAvailable for Mac, Windows and Chrome
// OS. This is exposed through the content public API for the purpose of
// reporting startup metrics.

using IsUVPlatformAuthenticatorAvailableCallback =
    base::OnceCallback<void(bool is_available)>;

#if defined(OS_MAC)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    const content::AuthenticatorRequestClientDelegate::
        TouchIdAuthenticatorConfig&,
    IsUVPlatformAuthenticatorAvailableCallback);

#elif defined(OS_WIN)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    device::WinWebAuthnApi*,
    IsUVPlatformAuthenticatorAvailableCallback);

#elif BUILDFLAG(IS_CHROMEOS_ASH)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);

#else
// Always returns false. On Android IsUVPlatformAuthenticatorAvailable() is
// called on GMSCore from Java and is not proxied from here because there could
// be performance costs to calling it outside of an actual WebAuthn API
// invocation.
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IS_UVPAA_H_
