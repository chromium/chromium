// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_SERVER_H_
#define REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_SERVER_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {
namespace test {

// Helper class to create a synchronous gRPC test server.
// Usage:
// 1. Define a mock/fake synchronous gRPC service class:
//
//      class MockTestService final : public MyService::Service {
//       public:
//        MOCK_METHOD3(MyTestMethod,
//                     grpc::Status(grpc::ServerContext*,
//                                  const MyTestRequest*,
//                                  MyTestResponse*));
//      };
//
// 2. Create a GrpcTestServer instance:
//
//      GrpcTestServer<MockTestService> test_server_;
//
// 3. Connect the channel:
//
//      my_object_.SetChannelForTest(test_server_.CreateInProcessChannel());
//
// 4. Use the `*` operator or get() to access the underlying service:
//
//      EXPECT_CALL(*test_server_, MyTestMethod(...)).WillOnce(...);
template <typename ServiceType>
class GrpcTestServer final {
 public:
  template <typename... Args>
  explicit GrpcTestServer(Args&&... args)
      : service_(std::forward<Args>(args)...) {
    grpc::ServerBuilder builder;
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
  }

  ~GrpcTestServer() = default;

  ServiceType& operator*() { return *get(); }
  ServiceType* get() { return &service_; }

  GrpcChannelSharedPtr CreateInProcessChannel() {
    return server_->InProcessChannel(grpc::ChannelArguments());
  }

 private:
  ServiceType service_;
  std::unique_ptr<grpc::Server> server_;
  DISALLOW_COPY_AND_ASSIGN(GrpcTestServer);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_SERVER_H_
