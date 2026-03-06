// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/federated_embedder_login_request.h"

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "url/origin.h"

namespace content::webid {

namespace {
constexpr base::TimeDelta kRequestTimeout = base::Seconds(20);
}  // namespace

FederatedEmbedderLoginRequest::FederatedEmbedderLoginRequest(
    WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    base::OnceCallback<void(FederatedLoginResult)> callback)
    : WebContentsUserData<FederatedEmbedderLoginRequest>(*web_contents),
      idp_origin_(idp_origin),
      account_id_(account_id),
      on_federated_result_received_callback_(std::move(callback)) {
  timeout_timer_.Start(FROM_HERE, kRequestTimeout,
                       base::BindOnce(&FederatedEmbedderLoginRequest::OnTimeout,
                                      base::Unretained(this)));
}

FederatedEmbedderLoginRequest::~FederatedEmbedderLoginRequest() = default;

void FederatedEmbedderLoginRequest::OnFederatedResultReceived(
    FederatedLoginResult result) {
  std::move(on_federated_result_received_callback_).Run(result);
  Unset();
}

void FederatedEmbedderLoginRequest::OnTimeout() {
  std::move(on_federated_result_received_callback_)
      .Run(FederatedLoginResult::kTimeout);
  Unset();
}

// static
void FederatedEmbedderLoginRequest::Set(
    WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    base::OnceCallback<void(FederatedLoginResult)> callback) {
  web_contents->SetUserData(
      FederatedEmbedderLoginRequest::UserDataKey(),
      std::make_unique<FederatedEmbedderLoginRequest>(
          web_contents, idp_origin, account_id, std::move(callback)));
}

void FederatedEmbedderLoginRequest::Unset() {
  GetWebContents().RemoveUserData(UserDataKey());
}

// static
FederatedEmbedderLoginRequest* FederatedEmbedderLoginRequest::Get(
    WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  FederatedEmbedderLoginRequest* request =
      WebContentsUserData<FederatedEmbedderLoginRequest>::FromWebContents(
          web_contents);
  if (request) {
    return request;
  }

  RenderFrameHost* opener_rfh = web_contents->GetOpener();
  if (!opener_rfh) {
    return nullptr;
  }

  return Get(WebContents::FromRenderFrameHost(opener_rfh));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FederatedEmbedderLoginRequest);

}  // namespace content::webid
