// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator_request_delegate.h"

#include <vector>

#include "base/callback.h"
#include "device/fido/authenticator_get_assertion_response.h"

namespace content {

VirtualAuthenticatorRequestDelegate::VirtualAuthenticatorRequestDelegate() =
    default;

VirtualAuthenticatorRequestDelegate::~VirtualAuthenticatorRequestDelegate() =
    default;

void VirtualAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  // TODO(crbug.com/991666): Provide a way to determine which account gets
  // picked.
  std::move(callback).Run(std::move(responses[0]));
}

}  // namespace content
