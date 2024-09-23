// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cloud_session_authz_service_client_factory.h"

#include <memory>

#include "base/notreached.h"
#include "remoting/base/cloud_service_client.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

class CloudSessionAuthzServiceClient : public SessionAuthzServiceClient {
 public:
  CloudSessionAuthzServiceClient(
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudSessionAuthzServiceClient(const CloudSessionAuthzServiceClient&) =
      delete;
  CloudSessionAuthzServiceClient& operator=(
      const CloudSessionAuthzServiceClient&) = delete;

  ~CloudSessionAuthzServiceClient() override;

  // SessionAuthzServiceClient implementation.
  void GenerateHostToken(GenerateHostTokenCallback callback) override;
  void VerifySessionToken(
      const internal::VerifySessionTokenRequestStruct& request,
      VerifySessionTokenCallback callback) override;
  void ReauthorizeHost(const internal::ReauthorizeHostRequestStruct& request,
                       ReauthorizeHostCallback callback) override;

 private:
  CloudServiceClient client_;
};

CloudSessionAuthzServiceClient::CloudSessionAuthzServiceClient(
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : client_(/*api_key=*/"", oauth_token_getter, url_loader_factory) {}

CloudSessionAuthzServiceClient::~CloudSessionAuthzServiceClient() = default;

void CloudSessionAuthzServiceClient::GenerateHostToken(
    GenerateHostTokenCallback callback) {
  NOTIMPLEMENTED();
}

void CloudSessionAuthzServiceClient::VerifySessionToken(
    const internal::VerifySessionTokenRequestStruct& request,
    VerifySessionTokenCallback callback) {
  NOTIMPLEMENTED();
}

void CloudSessionAuthzServiceClient::ReauthorizeHost(
    const internal::ReauthorizeHostRequestStruct& request,
    ReauthorizeHostCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace

CloudSessionAuthzServiceClientFactory::CloudSessionAuthzServiceClientFactory(
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : oauth_token_getter_(oauth_token_getter),
      url_loader_factory_(url_loader_factory) {}

CloudSessionAuthzServiceClientFactory::
    ~CloudSessionAuthzServiceClientFactory() = default;

std::unique_ptr<SessionAuthzServiceClient>
CloudSessionAuthzServiceClientFactory::Create() {
  return std::make_unique<CloudSessionAuthzServiceClient>(oauth_token_getter_,
                                                          url_loader_factory_);
}

}  // namespace remoting
