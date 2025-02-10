// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_session_authz_service_client_factory.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/corp_auth_util.h"
#include "remoting/base/corp_session_authz_service_client.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/oauth_token_getter_proxy.h"

namespace remoting {

CorpSessionAuthzServiceClientFactory::CorpSessionAuthzServiceClientFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateClientCertStoreCallback create_client_cert_store,
    const std::string& service_account_email,
    const std::string& refresh_token) {
  oauth_token_getter_for_service_account_ = CreateCorpTokenGetter(
      url_loader_factory, service_account_email, refresh_token);
  InitializeFactory(url_loader_factory, create_client_cert_store,
                    oauth_token_getter_for_service_account_->GetWeakPtr());
}

CorpSessionAuthzServiceClientFactory::CorpSessionAuthzServiceClientFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateClientCertStoreCallback create_client_cert_store,
    base::WeakPtr<OAuthTokenGetter> oauth_token_getter,
    std::string_view support_id) {
  support_id_ = support_id;
  InitializeFactory(url_loader_factory, create_client_cert_store,
                    oauth_token_getter);
}

void CorpSessionAuthzServiceClientFactory::InitializeFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateClientCertStoreCallback create_client_cert_store,
    base::WeakPtr<OAuthTokenGetter> oauth_token_getter) {
  DCHECK(url_loader_factory);

  url_loader_factory_ = url_loader_factory;
  create_client_cert_store_ = std::move(create_client_cert_store);
  oauth_token_getter_ = oauth_token_getter;
  oauth_token_getter_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
}

CorpSessionAuthzServiceClientFactory::~CorpSessionAuthzServiceClientFactory() =
    default;

std::unique_ptr<SessionAuthzServiceClient>
CorpSessionAuthzServiceClientFactory::Create() {
  return std::make_unique<CorpSessionAuthzServiceClient>(
      url_loader_factory_, create_client_cert_store_.Run(),
      std::make_unique<OAuthTokenGetterProxy>(oauth_token_getter_,
                                              oauth_token_getter_task_runner_),
      support_id_);
}

AuthenticationMethod CorpSessionAuthzServiceClientFactory::method() {
  return AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
}

}  // namespace remoting
