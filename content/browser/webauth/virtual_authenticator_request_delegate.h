// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "content/public/browser/authenticator_request_client_delegate.h"

namespace content {

class FrameTreeNode;

// An implementation of AuthenticatorRequestClientDelegate that allows
// automating webauthn requests through a virtual environment.
class VirtualAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  // The |frame_tree_node| must outlive this instance.
  explicit VirtualAuthenticatorRequestDelegate(FrameTreeNode* frame_tree_node);
  ~VirtualAuthenticatorRequestDelegate() override;

  // AuthenticatorRequestClientDelegate:
  bool SupportsResidentKeys() override;
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override;
  base::Optional<bool> IsUserVerifyingPlatformAuthenticatorAvailableOverride()
      override;

 private:
  FrameTreeNode* const frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAuthenticatorRequestDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_REQUEST_DELEGATE_H_
