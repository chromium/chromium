// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <vector>

#include "base/callback_forward.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "device/fido/authenticator_get_assertion_response.h"

namespace content {

// An implementation of AuthenticatorRequestClientDelegate that allows
// automating webauthn requests through a virtual environment.
class VirtualAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  // The |frame_tree_node| must outlive this instance.
  VirtualAuthenticatorRequestDelegate();

  VirtualAuthenticatorRequestDelegate(
      const VirtualAuthenticatorRequestDelegate&) = delete;
  VirtualAuthenticatorRequestDelegate& operator=(
      const VirtualAuthenticatorRequestDelegate&) = delete;

  ~VirtualAuthenticatorRequestDelegate() override;

  // AuthenticatorRequestClientDelegate:
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_
