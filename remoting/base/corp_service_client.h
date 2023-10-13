// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SERVICE_CLIENT_H_
#define REMOTING_BASE_CORP_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/protobuf_http_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/proto/helpers.h"
#else
#include "remoting/proto/internal_stubs.h"  // nogncheck
#endif

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

class ProtobufHttpStatus;

// A helper class that communicates with backend services using the Corp API.
class CorpServiceClient {
 public:
  using ProvisionCorpMachineCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::RemoteAccessHostV1Proto>)>;

  explicit CorpServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~CorpServiceClient();

  CorpServiceClient(const CorpServiceClient&) = delete;
  CorpServiceClient& operator=(const CorpServiceClient&) = delete;

  void ProvisionCorpMachine(const std::string& owner_email,
                            const std::string& fqdn,
                            const std::string& public_key,
                            absl::optional<std::string> existing_host_id,
                            ProvisionCorpMachineCallback callback);

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
