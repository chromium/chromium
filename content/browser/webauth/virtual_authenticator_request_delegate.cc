// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator_request_delegate.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/fido_transport_protocol.h"

namespace content {

VirtualAuthenticatorRequestDelegate::VirtualAuthenticatorRequestDelegate(
    FrameTreeNode* frame_tree_node)
    : frame_tree_node_(frame_tree_node) {}

VirtualAuthenticatorRequestDelegate::~VirtualAuthenticatorRequestDelegate() =
    default;

bool VirtualAuthenticatorRequestDelegate::SupportsResidentKeys() {
  return true;
}

void VirtualAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  // TODO(crbug.com/991666): Provide a way to determine which account gets
  // picked.
  std::move(callback).Run(std::move(responses[0]));
}

bool VirtualAuthenticatorRequestDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailable() {
  auto* virtual_discovery_factory =
      AuthenticatorEnvironmentImpl::GetInstance()->GetVirtualFactoryFor(
          frame_tree_node_);
  if (!virtual_discovery_factory) {
    return false;
  }
  const auto& authenticators = virtual_discovery_factory->GetAuthenticators();
  return std::any_of(authenticators.begin(), authenticators.end(),
                     [](VirtualAuthenticator* a) {
                       return a->is_user_verifying_platform_authenticator();
                     });
}

}  // namespace content
