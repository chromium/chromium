// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
#define REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/session_authz_service_client_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetterImpl;
class SessionAuthzServiceClient;

// SessionAuthzServiceClientFactory implementation that creates Corp
// SessionAuthz service clients.
class CorpSessionAuthzServiceClientFactory
    : public SessionAuthzServiceClientFactory {
 public:
  CorpSessionAuthzServiceClientFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& service_account_email,
      const std::string& refresh_token);

  CorpSessionAuthzServiceClientFactory(
      const CorpSessionAuthzServiceClientFactory&) = delete;
  CorpSessionAuthzServiceClientFactory& operator=(
      const CorpSessionAuthzServiceClientFactory&) = delete;

  std::unique_ptr<SessionAuthzServiceClient> Create() override;
  AuthenticationMethod method() override;

 private:
  ~CorpSessionAuthzServiceClientFactory() override;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<OAuthTokenGetterImpl> oauth_token_getter_;
  scoped_refptr<base::SequencedTaskRunner> oauth_token_getter_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
