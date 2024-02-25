// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_session_authz_service_client_factory.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/corp_session_authz_service_client.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/oauth_token_getter_proxy.h"

namespace remoting {
namespace {

constexpr char kOAuthScope[] =
    "https://www.googleapis.com/auth/chromoting.me2me.host";

}  // namespace

CorpSessionAuthzServiceClientFactory::CorpSessionAuthzServiceClientFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& service_account_email,
    const std::string& refresh_token) {
  DCHECK(url_loader_factory);
  DCHECK(!service_account_email.empty());
  DCHECK(!refresh_token.empty());

  url_loader_factory_ = url_loader_factory;
  oauth_token_getter_ = std::make_unique<OAuthTokenGetterImpl>(
      std::make_unique<OAuthTokenGetter::OAuthAuthorizationCredentials>(
          service_account_email, refresh_token,
          /* is_service_account= */ true,
          std::vector<std::string>{kOAuthScope}),
      url_loader_factory_,
      /* auto_refresh= */ false);
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

}  // namespace remoting
