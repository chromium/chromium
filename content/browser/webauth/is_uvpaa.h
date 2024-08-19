// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
#define CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_MAC)
class BrowserContext;
#endif

namespace content {

// This file contains platform-specific implementations of the
// IsUserVerifyingPlatformAuthenticatorAvailable interface.

using IsUVPlatformAuthenticatorAvailableCallback =
    base::OnceCallback<void(bool is_available)>;

#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback);
#elif BUILDFLAG(IS_WIN)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);
#elif BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
