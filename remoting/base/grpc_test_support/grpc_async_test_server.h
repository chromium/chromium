// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_ASYNC_TEST_SERVER_H_
#define REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_ASYNC_TEST_SERVER_H_

#include <memory>

#include "base/macros.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/grpc_test_support/grpc_test_util.h"
#include "third_party/grpc/src/include/grpcpp/impl/codegen/service_type.h"
#include "third_party/grpc/src/include/grpcpp/server.h"

namespace grpc {
class Service;
}  // namespace grpc

namespace grpc_impl {
class ServerCompletionQueue;
}  // namespace grpc_impl

namespace remoting {
namespace test {

// Helper class to allow mocking an async server and creating in process
// channel.
class GrpcAsyncTestServer {
 public:
  template <typename AsyncServiceType,
            typename RequestType,
            typename ResponseType>
  using AsyncRequestFuncPtr = void (AsyncServiceType::*)(
      grpc::ServerContext*,
      RequestType*,
      grpc_impl::ServerAsyncResponseWriter<ResponseType>*,
      grpc::CompletionQueue*,
      grpc::ServerCompletionQueue*,
      void*);
  template <typename AsyncServiceType,
            typename RequestType,
            typename ResponseType>
  using AsyncServerStreamingRequestFuncPtr =
      void (AsyncServiceType::*)(grpc::ServerContext*,
                                 RequestType*,
                                 grpc_impl::ServerAsyncWriter<ResponseType>*,
                                 grpc::CompletionQueue*,
                                 grpc::ServerCompletionQueue*,
                                 void*);

  explicit GrpcAsyncTestServer(std::unique_ptr<grpc::Service> async_service);
  virtual ~GrpcAsyncTestServer();

  GrpcChannelSharedPtr CreateInProcessChannel();

  // Accepts a request by calling |request_func|, writes the request to
  // |out_request|, and returns a responder for sending response to the client.
  template <typename AsyncServiceType,
            typename RequestType,
            typename ResponseType>
  std::unique_ptr<GrpcServerResponder<ResponseType>> HandleRequest(
      AsyncRequestFuncPtr<AsyncServiceType, RequestType, ResponseType>
          request_func,
      RequestType* out_request) {
    auto responder = std::make_unique<GrpcServerResponder<ResponseType>>(
        completion_queue_.get());
    AsyncServiceType* async_service =
        static_cast<AsyncServiceType*>(async_service_.get());
    (async_service->*request_func)(
        responder->context(), out_request, responder->writer(),
        completion_queue_.get(), completion_queue_.get(), /* event_tag */ this);
    WaitForCompletionAndAssertOk(FROM_HERE, completion_queue_.get(), this);
    return responder;
  }

  // Accepts a request by calling |request_func|, writes the request to
  // |out_request|, and returns a stream responder for sending response to the
  // client.
  template <typename AsyncServiceType,
            typename RequestType,
            typename ResponseType>
  std::unique_ptr<GrpcServerStreamResponder<ResponseType>> HandleStreamRequest(
      AsyncServerStreamingRequestFuncPtr<AsyncServiceType,
                                         RequestType,
                                         ResponseType> request_func,
      RequestType* out_request) {
    auto responder = std::make_unique<GrpcServerStreamResponder<ResponseType>>(
        completion_queue_.get());
    AsyncServiceType* async_service =
        static_cast<AsyncServiceType*>(async_service_.get());
    (async_service->*request_func)(
        responder->context(), out_request, responder->writer(),
        completion_queue_.get(), completion_queue_.get(), /* event_tag */ this);
    WaitForCompletionAndAssertOk(FROM_HERE, completion_queue_.get(), this);
    return responder;
  }

 private:
  std::unique_ptr<grpc::Service> async_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<grpc_impl::ServerCompletionQueue> completion_queue_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncTestServer);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_ASYNC_TEST_SERVER_H_
