// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_DEFAULT_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_BROWSER_WEBAUTH_DEFAULT_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

class WebContents;
// The default implementation of AuthenticatorRequestClientDelegate.
// It is used by embedders that do not provide their own implementation, e.g.
// content_shell. It provides no UI and has minimal functionality.

class CONTENT_EXPORT DefaultAuthenticatorRequestClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  DefaultAuthenticatorRequestClientDelegate();

  ~DefaultAuthenticatorRequestClientDelegate() override;

  DefaultAuthenticatorRequestClientDelegate(
      const DefaultAuthenticatorRequestClientDelegate&) = delete;
  DefaultAuthenticatorRequestClientDelegate& operator=(
      const DefaultAuthenticatorRequestClientDelegate&) = delete;

  // AuthenticatorRequestClientDelegate:
  void StartObserving(device::FidoRequestHandlerBase* request_handler) override;
  void StopObserving(device::FidoRequestHandlerBase* request_handler) override;

 private:
  base::ScopedObservation<device::FidoRequestHandlerBase,
                          device::FidoRequestHandlerBase::Observer>
      request_handler_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_DEFAULT_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
