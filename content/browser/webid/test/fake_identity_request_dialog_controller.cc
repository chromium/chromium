// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/fake_identity_request_dialog_controller.h"

#include "base/bind.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    absl::optional<UserApproval> initial_permission_response,
    absl::optional<UserApproval> token_exchange_permission_response,
    std::string id_token)
    : initial_permission_response_(initial_permission_response),
      token_exchange_permission_response_(token_exchange_permission_response),
      id_token_(id_token) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

void FakeIdentityRequestDialogController::ShowInitialPermissionDialog(
    WebContents*,
    const GURL&,
    PermissionDialogMode mode,
    InitialApprovalCallback callback) {
  if (initial_permission_response_)
    std::move(callback).Run(*initial_permission_response_);
}

void FakeIdentityRequestDialogController::ShowIdProviderWindow(
    WebContents*,
    WebContents* idp_web_contents,
    const GURL&,
    IdProviderWindowClosedCallback callback) {
  close_idp_window_callback_ = std::move(callback);
  auto* request_callback_data =
      IdTokenRequestCallbackData::Get(idp_web_contents);
  EXPECT_TRUE(request_callback_data);

  // TODO(kenrb, majidvp): This is faking the IdP response which in reality
  // comes from the navigator.id.provide() API call. We should instead load
  // the IdP page in the new WebContents and that API's behavior.
  auto rp_done_callback = request_callback_data->TakeDoneCallback();
  IdTokenRequestCallbackData::Remove(idp_web_contents);
  EXPECT_TRUE(rp_done_callback);
  std::move(rp_done_callback).Run(id_token_);
}

void FakeIdentityRequestDialogController::CloseIdProviderWindow() {
  std::move(close_idp_window_callback_).Run();
}

void FakeIdentityRequestDialogController::ShowTokenExchangePermissionDialog(
    content::WebContents*,
    const GURL&,
    TokenExchangeApprovalCallback callback) {
  if (token_exchange_permission_response_)
    std::move(callback).Run(*token_exchange_permission_response_);
}

}  // namespace content
