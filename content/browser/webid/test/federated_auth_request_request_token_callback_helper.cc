// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/federated_auth_request_request_token_callback_helper.h"

namespace content {

FederatedAuthRequestRequestTokenCallbackHelper::
    FederatedAuthRequestRequestTokenCallbackHelper() = default;
FederatedAuthRequestRequestTokenCallbackHelper::
    ~FederatedAuthRequestRequestTokenCallbackHelper() = default;

void FederatedAuthRequestRequestTokenCallbackHelper::WaitForCallback() {
  if (was_called_) {
    return;
  }
  wait_for_callback_loop_.Run();
}

void FederatedAuthRequestRequestTokenCallbackHelper::Reset() {
  status_.reset();
  selected_idp_config_url_.reset();
  token_.reset();
  was_called_ = false;
  wait_for_callback_loop_.Quit();
}

void FederatedAuthRequestRequestTokenCallbackHelper::ReceiverMethod(
    blink::mojom::RequestTokenStatus status,
    const absl::optional<GURL>& selected_idp_config_url,
    const absl::optional<std::string>& token,
    bool is_account_auto_selected) {
  CHECK(!was_called_);
  status_ = status;
  selected_idp_config_url_ = selected_idp_config_url;
  token_ = token;
  is_account_auto_selected_ = is_account_auto_selected;
  was_called_ = true;
  wait_for_callback_loop_.Quit();
}

void FederatedAuthRequestRequestTokenCallbackHelper::Quit() {
  wait_for_callback_loop_.Quit();
}

}  // namespace content
