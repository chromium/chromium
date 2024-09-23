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
    const std::string& service_account_email,
    const std::string& refresh_token) {
  DCHECK(url_loader_factory);

  url_loader_factory_ = url_loader_factory;
  oauth_token_getter_ = CreateCorpTokenGetter(
      url_loader_factory, service_account_email, refresh_token);
  oauth_token_getter_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
}

CorpSessionAuthzServiceClientFactory::~CorpSessionAuthzServiceClientFactory() =
    default;

std::unique_ptr<SessionAuthzServiceClient>
CorpSessionAuthzServiceClientFactory::Create() {
  return std::make_unique<CorpSessionAuthzServiceClient>(
      url_loader_factory_,
      std::make_unique<OAuthTokenGetterProxy>(oauth_token_getter_->GetWeakPtr(),
                                              oauth_token_getter_task_runner_));
}

AuthenticationMethod CorpSessionAuthzServiceClientFactory::method() {
  return AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
}

}  // namespace remoting
