// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_

#include "base/callback_forward.h"

namespace content {

// WebAuthentcationRequestProxy allows the embedder to intercept and handle Web
// Authentication API requests.
class WebAuthenticationRequestProxy {
 public:
  // IsUvpaaCallback is the response callback type for `SignalIsUvpaaRequest`.
  // It is invoked with the result of the proxied request.
  using IsUvpaaCallback = base::OnceCallback<void(bool is_available)>;

  virtual ~WebAuthenticationRequestProxy() = default;

  // IsActive indicates whether the proxy expects to handle Web Authentication
  // API requests.
  virtual bool IsActive() = 0;

  // SignalIsUvpaaRequest is invoked when a
  // PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable() (aka
  // `IsUvpaa`) request occurs.
  virtual void SignalIsUvpaaRequest(IsUvpaaCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_
