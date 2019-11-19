// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_UTIL_H_
#define REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_UTIL_H_

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace base {
class RunLoop;
}  // namespace base

namespace remoting {
namespace test {

// Block and wait until an event is received from the completion queue, and
// check if the tag matches |expected_tag|.
// Returns whether the event is marked "ok".
bool WaitForCompletion(const base::Location& from_here,
                       grpc_impl::CompletionQueue* completion_queue,
                       void* expected_tag);
void WaitForCompletionAndAssertOk(const base::Location& from_here,
                                  grpc_impl::CompletionQueue* completion_queue,
                                  void* expected_tag);

base::OnceCallback<void(const grpc::Status&)>
CheckStatusThenQuitRunLoopCallback(const base::Location& from_here,
                                   grpc::StatusCode expected_status_code,
                                   base::RunLoop* run_loop);

// Helper class for responding to an async server request.
template <typename ResponseType>
class GrpcServerResponder {
 public:
  explicit GrpcServerResponder(
      grpc_impl::ServerCompletionQueue* completion_queue) {
    completion_queue_ = completion_queue;
  }

  ~GrpcServerResponder() = default;

  bool Respond(const ResponseType& response, const grpc::Status& status) {
    writer_.Finish(response, status, this);
    return WaitForCompletion(FROM_HERE, completion_queue_, this);
  }

  grpc_impl::ServerContext* context() { return &context_; }

  grpc_impl::ServerAsyncResponseWriter<ResponseType>* writer() {
    return &writer_;
  }

 private:
  grpc_impl::ServerContext context_;
  grpc_impl::ServerCompletionQueue* completion_queue_ = nullptr;
  grpc_impl::ServerAsyncResponseWriter<ResponseType> writer_{&context_};

  DISALLOW_COPY_AND_ASSIGN(GrpcServerResponder);
};

// Helper class for responding to an async server stream request.
template <typename ResponseType>
class GrpcServerStreamResponder {
 public:
  explicit GrpcServerStreamResponder(
      grpc_impl::ServerCompletionQueue* completion_queue) {
    completion_queue_ = completion_queue;
  }

  ~GrpcServerStreamResponder() { Close(grpc::Status::OK); }

  // Must call WaitForSendMessageResult() once the client has received the
  // message.
  void SendMessage(const ResponseType& response) {
    writer_.Write(response, /* event_tag */ this);
  }

  // Returns true if the client requests for more messages, false if the client
  // has stopped the stream.
  bool WaitForSendMessageResult() {
    return WaitForCompletion(FROM_HERE, completion_queue_, this);
  }

  void Close(const grpc::Status& status) {
    if (closed_) {
      return;
    }
    writer_.Finish(status, /* event_tag */ this);
    bool ok = WaitForCompletion(FROM_HERE, completion_queue_, this);
    if (!ok) {
      LOG(WARNING) << "Failed to finish stream. Connection might be dropped.";
    }
    closed_ = true;
  }

  grpc_impl::ServerContext* context() { return &context_; }

  grpc_impl::ServerAsyncWriter<ResponseType>* writer() { return &writer_; }

 private:
  grpc_impl::ServerContext context_;
  grpc_impl::ServerCompletionQueue* completion_queue_ = nullptr;
  grpc_impl::ServerAsyncWriter<ResponseType> writer_{&context_};
  bool closed_ = false;

  DISALLOW_COPY_AND_ASSIGN(GrpcServerStreamResponder);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_TEST_SUPPORT_GRPC_TEST_UTIL_H_
