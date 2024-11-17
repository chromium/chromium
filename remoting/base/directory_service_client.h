// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DIRECTORY_SERVICE_CLIENT_H_
#define REMOTING_BASE_DIRECTORY_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_client.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

namespace apis {
namespace v1 {

class DeleteHostResponse;
class GetHostListResponse;
class HeartbeatResponse;
class RegisterHostResponse;
class SendHeartbeatResponse;

}  // namespace v1
}  // namespace apis

class OAuthTokenGetter;
class ProtobufHttpStatus;

// A service client that communicates with the directory service.
class DirectoryServiceClient {
 public:
  using DeleteHostCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<apis::v1::DeleteHostResponse>)>;
  using GetHostListCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<apis::v1::GetHostListResponse>)>;
  using LegacyHeartbeatCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<apis::v1::HeartbeatResponse>)>;
  using RegisterHostCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<apis::v1::RegisterHostResponse>)>;
  using SendHeartbeatCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<apis::v1::SendHeartbeatResponse>)>;

  DirectoryServiceClient(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~DirectoryServiceClient();

  DirectoryServiceClient(const DirectoryServiceClient&) = delete;
  DirectoryServiceClient& operator=(const DirectoryServiceClient&) = delete;

  void DeleteHost(const std::string& host_id, DeleteHostCallback callback);
  void GetHostList(GetHostListCallback callback);
  void LegacyHeartbeat(const std::string& directory_id,
                       std::optional<std::string> signaling_id,
                       std::optional<std::string> offline_reason,
                       bool is_initial_heartbeat,
                       bool set_fqdn,
                       const std::string& os_name,
                       const std::string& os_version,
                       LegacyHeartbeatCallback callback);
  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    RegisterHostCallback callback);
  void SendHeartbeat(const std::string& directory_id,
                     SendHeartbeatCallback callback);

  void CancelPendingRequests();

 private:
  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& path,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DIRECTORY_SERVICE_CLIENT_H_
