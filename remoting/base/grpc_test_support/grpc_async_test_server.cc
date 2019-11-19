// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_test_support/grpc_async_test_server.h"

#include <utility>

#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {
namespace test {

GrpcAsyncTestServer::GrpcAsyncTestServer(
    std::unique_ptr<grpc::Service> async_service) {
  async_service_ = std::move(async_service);
  grpc::ServerBuilder builder;
  builder.RegisterService(async_service_.get());
  completion_queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
}

GrpcAsyncTestServer::~GrpcAsyncTestServer() {
  server_->Shutdown();
  completion_queue_->Shutdown();

  // gRPC requires draining the completion queue before destroying it.
  void* tag;
  bool ok;
  while (completion_queue_->Next(&tag, &ok)) {
  }
}

GrpcChannelSharedPtr GrpcAsyncTestServer::CreateInProcessChannel() {
  return server_->InProcessChannel(grpc::ChannelArguments());
}

}  // namespace test
}  // namespace remoting
