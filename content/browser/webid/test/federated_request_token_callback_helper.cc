// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/federated_request_token_callback_helper.h"

namespace content {

FederatedRequestTokenCallbackHelper::FederatedRequestTokenCallbackHelper() =
    default;
FederatedRequestTokenCallbackHelper::~FederatedRequestTokenCallbackHelper() =
    default;

void FederatedRequestTokenCallbackHelper::WaitForCallback() {
  if (was_called_) {
    return;
  }
  wait_for_callback_loop_.Run();
}

void FederatedRequestTokenCallbackHelper::ReceiverMethod(
    base::expected<blink::mojom::TokenRequestSuccessPtr,
                   blink::mojom::TokenRequestFailurePtr> result) {
  CHECK(!was_called_);
  if (result.has_value()) {
    status_ = blink::mojom::RequestTokenStatus::kSuccess;
    selected_idp_config_url_ = result.value()->selected_idp_config_url;
    token_ = std::move(result.value()->token);
    is_auto_selected_ = result.value()->is_auto_selected;
  } else {
    status_ = result.error()->status;
    error_ = std::move(result.error()->error);
  }
  was_called_ = true;
  wait_for_callback_loop_.Quit();
}

void FederatedRequestTokenCallbackHelper::Quit() {
  wait_for_callback_loop_.Quit();
}

}  // namespace content
