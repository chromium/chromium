// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
#define REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_client.h"

namespace google::internal::remoting::cloud::v1alpha {
class Empty;
class GenerateHostTokenResponse;
class GenerateIceConfigResponse;
class ProvisionGceInstanceResponse;
class ReauthorizeHostResponse;
class RemoteAccessHost;
class VerifySessionTokenResponse;
}  // namespace google::internal::remoting::cloud::v1alpha

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

namespace apis::v1 {
class ProvisionGceInstanceResponse;
}  // namespace apis::v1

class ProtobufHttpStatus;

// A service client that communicates with the directory service.
class CloudServiceClient {
 public:
  using GenerateHostTokenCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          GenerateHostTokenResponse>)>;
  using GenerateIceConfigCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          GenerateIceConfigResponse>)>;
  using LegacyProvisionGceInstanceCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<apis::v1::ProvisionGceInstanceResponse>)>;
  using ProvisionGceInstanceCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          ProvisionGceInstanceResponse>)>;
  using ReauthorizeHostCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          ReauthorizeHostResponse>)>;
  using SendHeartbeatCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::Empty>)>;
  using UpdateRemoteAccessHostCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<
          ::google::internal::remoting::cloud::v1alpha::RemoteAccessHost>)>;
  using VerifySessionTokenCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          VerifySessionTokenResponse>)>;

  // TODO: joedow - Remove the single param c'tor when we no longer support the
  // legacy provisioning path.
  explicit CloudServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  CloudServiceClient(
      const std::string& api_key,
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~CloudServiceClient();

  CloudServiceClient(const CloudServiceClient&) = delete;
  CloudServiceClient& operator=(const CloudServiceClient&) = delete;

  // TODO: joedow - Remove the legacy codepath once the new flow is working.
  void LegacyProvisionGceInstance(
      const std::string& owner_email,
      const std::string& display_name,
      const std::string& public_key,
      const std::optional<std::string>& existing_directory_id,
      LegacyProvisionGceInstanceCallback callback);

  void ProvisionGceInstance(
      const std::string& owner_email,
      const std::string& display_name,
      const std::string& public_key,
      const std::optional<std::string>& existing_directory_id,
      ProvisionGceInstanceCallback callback);

  void SendHeartbeat(const std::string& directory_id,
                     SendHeartbeatCallback callback);

  void UpdateRemoteAccessHost(const std::string& directory_id,
                              std::optional<std::string> host_version,
                              std::optional<std::string> signaling_id,
                              std::optional<std::string> offline_reason,
                              std::optional<std::string> os_name,
                              std::optional<std::string> os_version,
                              UpdateRemoteAccessHostCallback callback);

  void GenerateIceConfig(GenerateIceConfigCallback callback);

  void GenerateHostToken(GenerateHostTokenCallback callback);

  void VerifySessionToken(const std::string& session_token,
                          VerifySessionTokenCallback callback);

  void ReauthorizeHost(const std::string& session_reauth_token,
                       const std::string& session_id,
                       ReauthorizeHostCallback callback);

  void CancelPendingRequests();

 private:
  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& path,
      const std::string& api_key,
      const std::string& method,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  // The customer API_KEY to use for calling the Cloud API.
  std::string api_key_;

  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
