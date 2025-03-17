// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
#define REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/corp_session_authz_service_client.h"
#include "remoting/base/session_authz_service_client_factory.h"

namespace net {
class ClientCertStore;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;
class OAuthTokenGetterImpl;
class SessionAuthzServiceClient;

// SessionAuthzServiceClientFactory implementation that creates Corp
// SessionAuthz service clients.
class CorpSessionAuthzServiceClientFactory
    : public SessionAuthzServiceClientFactory {
 public:
  using CreateClientCertStoreCallback =
      base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>()>;

  CorpSessionAuthzServiceClientFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CreateClientCertStoreCallback create_client_cert_store,
      const std::string& service_account_email,
      const std::string& refresh_token);

  // |support_id|: The 7-digit support ID.
  CorpSessionAuthzServiceClientFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CreateClientCertStoreCallback create_client_cert_store,
      base::WeakPtr<OAuthTokenGetter> oauth_token_getter,
      std::string_view support_id);

  CorpSessionAuthzServiceClientFactory(
      const CorpSessionAuthzServiceClientFactory&) = delete;
  CorpSessionAuthzServiceClientFactory& operator=(
      const CorpSessionAuthzServiceClientFactory&) = delete;

  std::unique_ptr<SessionAuthzServiceClient> Create() override;
  AuthenticationMethod method() override;

 private:
  ~CorpSessionAuthzServiceClientFactory() override;

  void InitializeFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CreateClientCertStoreCallback create_client_cert_store,
      base::WeakPtr<OAuthTokenGetter> oauth_token_getter);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  CreateClientCertStoreCallback create_client_cert_store_;

  // This is nullptr if the factory is not constructed with a service account.
  std::unique_ptr<OAuthTokenGetterImpl> oauth_token_getter_for_service_account_;

  base::WeakPtr<OAuthTokenGetter> oauth_token_getter_;
  scoped_refptr<base::SequencedTaskRunner> oauth_token_getter_task_runner_;
  std::string support_id_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
