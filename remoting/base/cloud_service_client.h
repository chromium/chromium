// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
#define REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_client.h"

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
  using ProvisionGceInstanceCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<apis::v1::ProvisionGceInstanceResponse>)>;

  explicit CloudServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~CloudServiceClient();

  CloudServiceClient(const CloudServiceClient&) = delete;
  CloudServiceClient& operator=(const CloudServiceClient&) = delete;

  void ProvisionGceInstance(
      const std::string& owner_email,
      const std::string& display_name,
      const std::string& public_key,
      const std::optional<std::string>& existing_directory_id,
      ProvisionGceInstanceCallback callback);

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

#endif  // REMOTING_BASE_CLOUD_SERVICE_CLIENT_H_
