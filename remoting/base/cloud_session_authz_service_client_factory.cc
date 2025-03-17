// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cloud_session_authz_service_client_factory.h"

#include <limits.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "remoting/base/cloud_service_client.h"
#include "remoting/base/instance_identity_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/duration.pb.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/session_authz_service.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

using Duration = google::internal::remoting::cloud::v1alpha::Duration;
using GenerateHostTokenResponse =
    google::internal::remoting::cloud::v1alpha::GenerateHostTokenResponse;
using ReauthorizeHostResponse =
    google::internal::remoting::cloud::v1alpha::ReauthorizeHostResponse;
using VerifySessionTokenResponse =
    google::internal::remoting::cloud::v1alpha::VerifySessionTokenResponse;

base::TimeDelta fromProtoDuration(const Duration& duration) {
  return base::Seconds(duration.seconds()) +
         base::Nanoseconds(duration.nanos());
}

class CloudSessionAuthzServiceClient : public SessionAuthzServiceClient {
 public:
  CloudSessionAuthzServiceClient(
      OAuthTokenGetter* oauth_token_getter,
      InstanceIdentityTokenGetter* instance_identity_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudSessionAuthzServiceClient(const CloudSessionAuthzServiceClient&) =
      delete;
  CloudSessionAuthzServiceClient& operator=(
      const CloudSessionAuthzServiceClient&) = delete;

  ~CloudSessionAuthzServiceClient() override;

  // SessionAuthzServiceClient implementation.
  void GenerateHostToken(GenerateHostTokenCallback callback) override;
  void VerifySessionToken(std::string_view session_token,
                          VerifySessionTokenCallback callback) override;
  void ReauthorizeHost(std::string_view session_reauth_token,
                       std::string_view session_id,
                       ReauthorizeHostCallback callback) override;

 private:
  // Overloads used to create callbacks for |instance_identity_token_getter_|.
  void GenerateHostTokenWithIdToken(GenerateHostTokenCallback callback,
                                    std::string_view instance_identity_token);
  void VerifySessionTokenWithIdToken(std::string session_token,
                                     VerifySessionTokenCallback callback,
                                     std::string_view instance_identity_token);
  void ReauthorizeHostWithIdToken(std::string session_reauth_token,
                                  std::string session_id,
                                  ReauthorizeHostCallback callback,
                                  std::string_view instance_identity_token);

  void OnGenerateHostTokenResponse(
      GenerateHostTokenCallback callback,
      const HttpStatus& status,
      std::unique_ptr<GenerateHostTokenResponse> response);
  void OnVerifySessionTokenResponse(
      VerifySessionTokenCallback callback,
      const HttpStatus& status,
      std::unique_ptr<VerifySessionTokenResponse> response);
  void OnReauthorizeHostResponse(
      ReauthorizeHostCallback callback,
      const HttpStatus& status,
      std::unique_ptr<ReauthorizeHostResponse> response);

  std::unique_ptr<CloudServiceClient> client_;
  const raw_ptr<InstanceIdentityTokenGetter> instance_identity_token_getter_;

  base::WeakPtrFactory<CloudSessionAuthzServiceClient> weak_factory_{this};
};

CloudSessionAuthzServiceClient::CloudSessionAuthzServiceClient(
    OAuthTokenGetter* oauth_token_getter,
    InstanceIdentityTokenGetter* instance_identity_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : client_(CloudServiceClient::CreateForChromotingRobotAccount(
          oauth_token_getter,
          url_loader_factory)),
      instance_identity_token_getter_(instance_identity_token_getter) {}

CloudSessionAuthzServiceClient::~CloudSessionAuthzServiceClient() = default;

void CloudSessionAuthzServiceClient::GenerateHostToken(
    GenerateHostTokenCallback callback) {
  instance_identity_token_getter_->RetrieveToken(base::BindOnce(
      &CloudSessionAuthzServiceClient::GenerateHostTokenWithIdToken,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudSessionAuthzServiceClient::GenerateHostTokenWithIdToken(
    GenerateHostTokenCallback callback,
    std::string_view instance_identity_token) {
  client_->GenerateHostToken(
      instance_identity_token,
      base::BindOnce(
          &CloudSessionAuthzServiceClient::OnGenerateHostTokenResponse,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudSessionAuthzServiceClient::VerifySessionToken(
    std::string_view session_token,
    VerifySessionTokenCallback callback) {
  instance_identity_token_getter_->RetrieveToken(base::BindOnce(
      &CloudSessionAuthzServiceClient::VerifySessionTokenWithIdToken,
      weak_factory_.GetWeakPtr(), std::string(session_token),
      std::move(callback)));
}

void CloudSessionAuthzServiceClient::VerifySessionTokenWithIdToken(
    std::string session_token,
    VerifySessionTokenCallback callback,
    std::string_view instance_identity_token) {
  client_->VerifySessionToken(
      session_token, instance_identity_token,
      base::BindOnce(
          &CloudSessionAuthzServiceClient::OnVerifySessionTokenResponse,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudSessionAuthzServiceClient::ReauthorizeHost(
    std::string_view session_reauth_token,
    std::string_view session_id,
    ReauthorizeHostCallback callback) {
  instance_identity_token_getter_->RetrieveToken(base::BindOnce(
      &CloudSessionAuthzServiceClient::ReauthorizeHostWithIdToken,
      weak_factory_.GetWeakPtr(), std::string(session_reauth_token),
      std::string(session_id), std::move(callback)));
}

void CloudSessionAuthzServiceClient::ReauthorizeHostWithIdToken(
    std::string session_reauth_token,
    std::string session_id,
    ReauthorizeHostCallback callback,
    std::string_view instance_identity_token) {
  client_->ReauthorizeHost(
      session_reauth_token, session_id, instance_identity_token,
      base::BindOnce(&CloudSessionAuthzServiceClient::OnReauthorizeHostResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudSessionAuthzServiceClient::OnGenerateHostTokenResponse(
    GenerateHostTokenCallback callback,
    const HttpStatus& status,
    std::unique_ptr<GenerateHostTokenResponse> response) {
  auto response_struct =
      std::make_unique<internal::GenerateHostTokenResponseStruct>();
  response_struct->host_token = response->host_token();
  response_struct->session_id = response->session_id();
  std::move(callback).Run(status, std::move(response_struct));
}

void CloudSessionAuthzServiceClient::OnVerifySessionTokenResponse(
    VerifySessionTokenCallback callback,
    const HttpStatus& status,
    std::unique_ptr<VerifySessionTokenResponse> response) {
  auto response_struct =
      std::make_unique<internal::VerifySessionTokenResponseStruct>();
  response_struct->session_id = response->session_id();
  response_struct->shared_secret = response->shared_secret();
  response_struct->session_reauth_token = response->session_reauth_token();
  auto token_lifetime = response->session_reauth_token_lifetime();
  response_struct->session_reauth_token_lifetime =
      fromProtoDuration(response->session_reauth_token_lifetime());
  if (response->has_session_policies()) {
    SessionPolicies session_policies;
    if (response->session_policies().has_allow_file_transfer()) {
      session_policies.allow_file_transfer =
          response->session_policies().allow_file_transfer();
    }
    if (response->session_policies().has_allow_relayed_connections()) {
      session_policies.allow_relayed_connections =
          response->session_policies().allow_relayed_connections();
    }
    if (response->session_policies().has_allow_stun_connections()) {
      session_policies.allow_stun_connections =
          response->session_policies().allow_stun_connections();
    }
    if (response->session_policies().has_allow_uri_forwarding()) {
      session_policies.allow_uri_forwarding =
          response->session_policies().allow_uri_forwarding();
    }
    if (response->session_policies().has_clipboard_size_bytes()) {
      session_policies.clipboard_size_bytes =
          response->session_policies().clipboard_size_bytes();
    }
    if (response->session_policies().has_curtain_required()) {
      session_policies.curtain_required =
          response->session_policies().curtain_required();
    }
    if (response->session_policies().has_host_udp_port_range()) {
      const auto& proto_port_range =
          response->session_policies().host_udp_port_range();
      int min_port =
          proto_port_range.has_start() ? proto_port_range.start() : 1;
      int max_port =
          proto_port_range.has_end() ? proto_port_range.end() : USHRT_MAX;
      if (min_port < 1 || min_port > max_port || max_port > USHRT_MAX) {
        LOG(ERROR) << "Invalid port range: [" << min_port << ", " << max_port
                   << "]";
      } else {
        session_policies.host_udp_port_range.min_port = min_port;
        session_policies.host_udp_port_range.max_port = max_port;
      }
    }
    response_struct->session_policies.emplace(std::move(session_policies));
  }

  std::move(callback).Run(status, std::move(response_struct));
}

void CloudSessionAuthzServiceClient::OnReauthorizeHostResponse(
    ReauthorizeHostCallback callback,
    const HttpStatus& status,
    std::unique_ptr<ReauthorizeHostResponse> response) {
  auto response_struct =
      std::make_unique<internal::ReauthorizeHostResponseStruct>();
  response_struct->session_reauth_token = response->session_reauth_token();
  response_struct->session_reauth_token_lifetime =
      fromProtoDuration(response->session_reauth_token_lifetime());
  std::move(callback).Run(status, std::move(response_struct));
}

}  // namespace

CloudSessionAuthzServiceClientFactory::CloudSessionAuthzServiceClientFactory(
    OAuthTokenGetter* oauth_token_getter,
    InstanceIdentityTokenGetter* instance_identity_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : oauth_token_getter_(oauth_token_getter),
      instance_identity_token_getter_(instance_identity_token_getter),
      url_loader_factory_(url_loader_factory) {}

CloudSessionAuthzServiceClientFactory::
    ~CloudSessionAuthzServiceClientFactory() = default;

std::unique_ptr<SessionAuthzServiceClient>
CloudSessionAuthzServiceClientFactory::Create() {
  return std::make_unique<CloudSessionAuthzServiceClient>(
      oauth_token_getter_, instance_identity_token_getter_,
      url_loader_factory_);
}

AuthenticationMethod CloudSessionAuthzServiceClientFactory::method() {
  return AuthenticationMethod::CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519;
}

}  // namespace remoting
