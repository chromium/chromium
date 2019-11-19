// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_DIRECTORY_CLIENT_H_
#define REMOTING_IOS_FACADE_DIRECTORY_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"

namespace remoting {

class GrpcExecutor;
class OAuthTokenGetter;

// A gRPC client that communicates with the directory service.
class DirectoryClient final {
 public:
  using GetHostListCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const apis::v1::GetHostListResponse&)>;

  // Creates a client that connects to the default server endpoint.
  explicit DirectoryClient(OAuthTokenGetter* token_getter);

  // Creates a client with custom executor and channel. Useful for testing.
  DirectoryClient(std::unique_ptr<GrpcExecutor> executor,
                  GrpcChannelSharedPtr channel);

  ~DirectoryClient();

  void GetHostList(GetHostListCallback callback);
  void CancelPendingRequests();

 private:
  using DirectoryService = apis::v1::RemotingDirectoryService;

  std::unique_ptr<GrpcExecutor> grpc_executor_;
  std::unique_ptr<DirectoryService::Stub> stub_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryClient);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_DIRECTORY_CLIENT_H_
