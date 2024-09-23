// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CLOUD_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
#define REMOTING_BASE_CLOUD_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/session_authz_service_client_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;
class SessionAuthzServiceClient;

// SessionAuthzServiceClientFactory implementation that creates SessionAuthz
// service clients for Cloud hosts.
class CloudSessionAuthzServiceClientFactory
    : public SessionAuthzServiceClientFactory {
 public:
  CloudSessionAuthzServiceClientFactory(
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudSessionAuthzServiceClientFactory(
      const CloudSessionAuthzServiceClientFactory&) = delete;
  CloudSessionAuthzServiceClientFactory& operator=(
      const CloudSessionAuthzServiceClientFactory&) = delete;

  std::unique_ptr<SessionAuthzServiceClient> Create() override;
  AuthenticationMethod method() override;

 private:
  ~CloudSessionAuthzServiceClientFactory() override;

  const raw_ptr<OAuthTokenGetter> oauth_token_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CLOUD_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
