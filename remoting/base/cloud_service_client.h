// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
#define REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_client.h"

namespace google::internal::remoting::cloud::v1alpha {
class Empty;
class GenerateHostTokenResponse;
class GenerateIceConfigResponse;
class ReauthorizeHostResponse;
class RemoteAccessHost;
class VerifySessionTokenResponse;
}  // namespace google::internal::remoting::cloud::v1alpha

namespace google::remoting::cloud::v1 {
class ProvisionGceInstanceResponse;
}  // namespace google::remoting::cloud::v1

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

class HttpStatus;

// A service client that communicates with the directory service.
class CloudServiceClient {
 public:
  using GenerateHostTokenCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          GenerateHostTokenResponse>)>;
  using GenerateIceConfigCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          GenerateIceConfigResponse>)>;
  using ProvisionGceInstanceCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<
          ::google::remoting::cloud::v1::ProvisionGceInstanceResponse>)>;
  using ReauthorizeHostCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          ReauthorizeHostResponse>)>;
  using SendHeartbeatCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::Empty>)>;
  using UpdateRemoteAccessHostCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<
          ::google::internal::remoting::cloud::v1alpha::RemoteAccessHost>)>;
  using VerifySessionTokenCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::
                          VerifySessionTokenResponse>)>;

  // Used for creating a service client to call the Remoting Cloud Private API
  // using a scoped OAuth access token generated for the device robot account.
  static std::unique_ptr<CloudServiceClient> CreateForChromotingRobotAccount(
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  // Used for creating a service client to call the Remoting Cloud API using
  // the |api_key| provided which associates the request with a GCP project.
  static std::unique_ptr<CloudServiceClient> CreateForGcpProject(
      const std::string& api_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  // Used for creating a service client to call the Remoting Cloud API using
  // an access token associated with the default service account for the
  // Compute Engine Instance the code is running on.
  static std::unique_ptr<CloudServiceClient> CreateForGceDefaultServiceAccount(
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~CloudServiceClient();

  CloudServiceClient(const CloudServiceClient&) = delete;
  CloudServiceClient& operator=(const CloudServiceClient&) = delete;

  void ProvisionGceInstance(
      const std::string& owner_email,
      const std::string& display_name,
      const std::string& public_key,
      const std::optional<std::string>& existing_directory_id,
      const std::optional<std::string>& instance_identity_token,
      ProvisionGceInstanceCallback callback);

  void SendHeartbeat(const std::string& directory_id,
                     std::string_view instance_identity_token,
                     SendHeartbeatCallback callback);

  void UpdateRemoteAccessHost(const std::string& directory_id,
                              std::optional<std::string> host_version,
                              std::optional<std::string> signaling_id,
                              std::optional<std::string> offline_reason,
                              std::optional<std::string> os_name,
                              std::optional<std::string> os_version,
                              std::string_view instance_identity_token,
                              UpdateRemoteAccessHostCallback callback);

  void GenerateIceConfig(std::string_view instance_identity_token,
                         GenerateIceConfigCallback callback);

  void GenerateHostToken(std::string_view instance_identity_token,
                         GenerateHostTokenCallback callback);

  void VerifySessionToken(const std::string& session_token,
                          std::string_view instance_identity_token,
                          VerifySessionTokenCallback callback);

  void ReauthorizeHost(const std::string& session_reauth_token,
                       const std::string& session_id,
                       std::string_view instance_identity_token,
                       ReauthorizeHostCallback callback);

  void CancelPendingRequests();

 private:
  CloudServiceClient(
      const std::string& api_key,
      OAuthTokenGetter* oauth_token_getter,
      const std::string& base_service_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& path,
      const std::string& api_key,
      const std::string& method,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  // The customer API_KEY to use for calling the Remoting Cloud API.
  std::string api_key_;

  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
