// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/default_authenticator_request_client_delegate.h"

#include "content/public/browser/web_contents.h"

namespace content {

DefaultAuthenticatorRequestClientDelegate::
    DefaultAuthenticatorRequestClientDelegate() = default;

DefaultAuthenticatorRequestClientDelegate::
    ~DefaultAuthenticatorRequestClientDelegate() = default;

void DefaultAuthenticatorRequestClientDelegate::StartObserving(
    device::FidoRequestHandlerBase* request_handler) {
  request_handler_observation_.Observe(request_handler);
}

void DefaultAuthenticatorRequestClientDelegate::StopObserving(
    device::FidoRequestHandlerBase* request_handler) {
  request_handler_observation_.Reset();
}

}  // namespace content
