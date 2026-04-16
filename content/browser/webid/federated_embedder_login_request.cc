// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/federated_embedder_login_request.h"

#include <string>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
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
      WebContentsObserver(web_contents),
      idp_origin_(idp_origin),
      account_id_(account_id),
      on_federated_result_received_callback_(std::move(callback)) {
  timeout_timer_.Start(FROM_HERE, kRequestTimeout,
                       base::BindOnce(&FederatedEmbedderLoginRequest::OnTimeout,
                                      base::Unretained(this)));
}

FederatedEmbedderLoginRequest::~FederatedEmbedderLoginRequest() {
  DCHECK(completion_callbacks_.empty());
}

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

void FederatedEmbedderLoginRequest::WillBeDestroyed() {
  completion_callbacks_.Notify();
}

// static
void FederatedEmbedderLoginRequest::Set(
    WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    base::OnceCallback<void(FederatedLoginResult)> callback) {
  Remove(web_contents);
  web_contents->SetUserData(
      FederatedEmbedderLoginRequest::UserDataKey(),
      std::make_unique<FederatedEmbedderLoginRequest>(
          web_contents, idp_origin, account_id, std::move(callback)));
}

void FederatedEmbedderLoginRequest::Unset() {
  WillBeDestroyed();
  GetWebContents().RemoveUserData(UserDataKey());
}

void FederatedEmbedderLoginRequest::WebContentsDestroyed() {
  WillBeDestroyed();
}

// static
void FederatedEmbedderLoginRequest::Remove(WebContents* web_contents) {
  if (web_contents) {
    if (auto* request = FromWebContents(web_contents)) {
      request->Unset();
    }
  }
}

// static
FederatedEmbedderLoginRequest* FederatedEmbedderLoginRequest::Get(
    WebContents* web_contents) {
  // There can be cycles in the opener chain; keep track of visited openers to
  // avoid getting stuck in a cycle.
  absl::flat_hash_set<WebContents*> visited;
  while (web_contents) {
    if (!visited.insert(web_contents).second) {
      return nullptr;
    }

    FederatedEmbedderLoginRequest* request = FromWebContents(web_contents);
    if (request) {
      return request;
    }

    RenderFrameHost* opener_rfh = web_contents->GetOpener();
    if (!opener_rfh) {
      return nullptr;
    }
    web_contents = WebContents::FromRenderFrameHost(opener_rfh);
  }
  return nullptr;
}

base::CallbackListSubscription
FederatedEmbedderLoginRequest::RegisterCompletion(base::OnceClosure callback) {
  return completion_callbacks_.Add(std::move(callback));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FederatedEmbedderLoginRequest);

}  // namespace content::webid
