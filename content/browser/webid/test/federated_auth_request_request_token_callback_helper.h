// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_FEDERATED_AUTH_REQUEST_REQUEST_TOKEN_CALLBACK_HELPER_H_
#define CONTENT_BROWSER_WEBID_TEST_FEDERATED_AUTH_REQUEST_REQUEST_TOKEN_CALLBACK_HELPER_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

// Helper class for waiting for the FederatedAuthRequestImpl::RequestToken()
// callback.
class FederatedAuthRequestRequestTokenCallbackHelper {
 public:
  FederatedAuthRequestRequestTokenCallbackHelper();
  ~FederatedAuthRequestRequestTokenCallbackHelper();

  FederatedAuthRequestRequestTokenCallbackHelper(
      const FederatedAuthRequestRequestTokenCallbackHelper&) = delete;
  FederatedAuthRequestRequestTokenCallbackHelper& operator=(
      const FederatedAuthRequestRequestTokenCallbackHelper&) = delete;

  absl::optional<blink::mojom::RequestTokenStatus> status() const {
    return status_;
  }
  absl::optional<GURL> selected_idp_config_url() const {
    return selected_idp_config_url_;
  }
  absl::optional<std::string> token() const { return token_; }

  // Returns base::OnceClosure which quits base::RunLoop started by
  // WaitForCallback().
  base::OnceClosure quit_closure() {
    return base::BindOnce(&FederatedAuthRequestRequestTokenCallbackHelper::Quit,
                          base::Unretained(this));
  }

  // This can only be called once per lifetime of this object.
  FederatedAuthRequestImpl::RequestTokenCallback callback() {
    return base::BindOnce(
        &FederatedAuthRequestRequestTokenCallbackHelper::ReceiverMethod,
        base::Unretained(this));
  }

  bool was_callback_called() const { return was_called_; }

  // Returns when callback() is called, which can be immediately if it has
  // already been called.
  void WaitForCallback();

  void Reset();

 private:
  void ReceiverMethod(blink::mojom::RequestTokenStatus status,
                      const absl::optional<GURL>& selected_idp_config_url,
                      const absl::optional<std::string>& token,
                      bool is_account_auto_selected);

  void Quit();

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  absl::optional<blink::mojom::RequestTokenStatus> status_;
  absl::optional<GURL> selected_idp_config_url_;
  absl::optional<std::string> token_;
  bool is_account_auto_selected_{false};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_FEDERATED_AUTH_REQUEST_REQUEST_TOKEN_CALLBACK_HELPER_H_
