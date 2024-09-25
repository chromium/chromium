// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SERVICE_CLIENT_H_
#define REMOTING_BASE_CORP_SERVICE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/proto/empty.pb.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

class OAuthTokenGetter;
class ProtobufHttpStatus;

// A helper class that communicates with backend services using the Corp API.
class CorpServiceClient {
 public:
  using ProvisionCorpMachineCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::ProvisionCorpMachineResponse>)>;
  using ReportProvisioningErrorCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<Empty>)>;
  using SendHeartbeatCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<Empty>)>;
  using UpdateRemoteAccessHostCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::RemoteAccessHostV1Proto>)>;

  // C'tor to use for unauthenticated service requests.
  explicit CorpServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  // C'tor to use for authenticated requests using the device robot account.
  CorpServiceClient(
      const std::string& refresh_token,
      const std::string& service_account_email,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~CorpServiceClient();

  CorpServiceClient(const CorpServiceClient&) = delete;
  CorpServiceClient& operator=(const CorpServiceClient&) = delete;

  void ProvisionCorpMachine(const std::string& owner_email,
                            const std::string& fqdn,
                            const std::string& public_key,
                            const std::optional<std::string>& existing_host_id,
                            ProvisionCorpMachineCallback callback);

  void ReportProvisioningError(const std::string& host_id,
                               const std::string& error_message,
                               ReportProvisioningErrorCallback callback);

  void SendHeartbeat(const std::string& directory_id,
                     SendHeartbeatCallback callback);

  void UpdateRemoteAccessHost(const std::string& directory_id,
                              std::optional<std::string> host_version,
                              std::optional<std::string> signaling_id,
                              std::optional<std::string> offline_reason,
                              std::optional<std::string> os_name,
                              std::optional<std::string> os_version,
                              UpdateRemoteAccessHostCallback callback);

  void CancelPendingRequests();

 private:
  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& path,
      const std::string& method,
      bool unauthenticated,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;
  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_SERVICE_CLIENT_H_
