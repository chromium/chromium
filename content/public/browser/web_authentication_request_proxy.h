// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

// WebAuthentcationRequestProxy allows the embedder to intercept and handle Web
// Authentication API requests.
class WebAuthenticationRequestProxy {
 public:
  // RequestId uniquely identifies a proxied request when invoking a response
  // callback or cancelling.
  using RequestId = int32_t;

  // CreateCallback is the response callback type for `SignalCreateRequest`. It
  // receives either the error or the response that resulted from the proxied
  // request.
  using CreateCallback = base::OnceCallback<void(
      RequestId,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr)>;

  // GetCallback is the response callback type for `SignalCreateRequest`. It
  // is invoked with the status and optional response that resulted from the
  // proxied request.
  using GetCallback = base::OnceCallback<void(
      RequestId,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr,
      blink::mojom::GetAssertionAuthenticatorResponsePtr)>;

  // IsUvpaaCallback is the response callback type for `SignalIsUvpaaRequest`.
  // It is invoked with the result of the proxied request.
  using IsUvpaaCallback = base::OnceCallback<void(bool is_available)>;

  virtual ~WebAuthenticationRequestProxy() = default;

  // IsActive indicates whether the proxy expects to handle Web Authentication
  // API requests for the given `caller_origin`.
  virtual bool IsActive(const url::Origin& caller_origin) = 0;

  // SignalCreateRequest is invoked when a Web Authentication API
  // `navigator.credentials.create()` request occurs.
  virtual RequestId SignalCreateRequest(
      const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options,
      CreateCallback callback) = 0;

  // SignalGetRequest is invoked when a Web Authentication API
  // `navigator.credentials.get()` request occurs.
  virtual RequestId SignalGetRequest(
      const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
      GetCallback callback) = 0;

  // SignalIsUvpaaRequest is invoked when a
  // PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable() (aka
  // `IsUvpaa`) request occurs.
  virtual RequestId SignalIsUvpaaRequest(IsUvpaaCallback callback) = 0;

  // CancelRequest cancels processing of the request with the
  // given `request_id`.
  virtual void CancelRequest(RequestId request_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_REQUEST_PROXY_H_
