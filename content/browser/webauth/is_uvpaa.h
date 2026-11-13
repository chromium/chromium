// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
#define CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;

using IsUVPlatformAuthenticatorAvailableCallback =
    base::OnceCallback<void(bool is_available)>;

// Checks whether a user-verifying platform authenticator is available and
// invokes `callback` with the result. `browser_context` is used on macOS to
// look up the Touch ID authenticator configuration. Invokes `callback` with
// false on unsupported platforms.
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback callback);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
