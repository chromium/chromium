// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "remoting/base/grpc_support/grpc_async_request.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"

namespace remoting {

template <typename RequestType, typename ResponseType>
using GrpcAsyncUnaryRpcFunction = base::OnceCallback<
    std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>(
        grpc::ClientContext*,
        const RequestType&,
        grpc_impl::CompletionQueue*)>;

template <typename ResponseType>
using GrpcAsyncUnaryRpcCallback =
    base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;

// GrpcAsyncRequest implementation for unary call. The object is enqueued
// when waiting for response and dequeued once the response is received.
template <typename ResponseType>
class GrpcAsyncUnaryRequest : public GrpcAsyncRequest {
 public:
  using StartAndCreateReaderCallback = base::OnceCallback<
      std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>(
          grpc_impl::CompletionQueue*)>;

  ~GrpcAsyncUnaryRequest() override = default;

 private:
  template <typename Req, typename Res>
  friend std::unique_ptr<GrpcAsyncUnaryRequest<Res>>
  CreateGrpcAsyncUnaryRequest(
      GrpcAsyncUnaryRpcFunction<Req, Res> rpc_function,
      const Req& request,
      base::OnceCallback<void(const grpc::Status&, const Res&)> callback);

  GrpcAsyncUnaryRequest() = default;

  void SetCallbacks(StartAndCreateReaderCallback create_reader_cb,
                    GrpcAsyncUnaryRpcCallback<ResponseType> callback) {
    DCHECK(!create_reader_cb_);
    DCHECK(!callback_);
    DCHECK(create_reader_cb);
    DCHECK(callback);
    create_reader_cb_ = std::move(create_reader_cb);
    callback_ = std::move(callback);
  }

  // GrpcAsyncRequest implementations
  void Start(const RunTaskCallback& run_task_cb,
             grpc_impl::CompletionQueue* cq,
             void* event_tag) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    static constexpr base::TimeDelta kDefaultRequestTimeout =
        base::TimeDelta::FromSeconds(30);
    if (GetDeadline(*context()).is_max()) {
      VLOG(1) << "Deadline is not set. Using the default request timeout.";
      SetDeadline(context(), base::Time::Now() + kDefaultRequestTimeout);
    }

    response_reader_ = std::move(create_reader_cb_).Run(cq);
    response_reader_->Finish(&response_, &status_, event_tag);
    run_task_cb_ = run_task_cb;
  }

  bool OnDequeue(bool operation_succeeded) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(operation_succeeded);
    DCHECK(callback_);
    run_task_cb_.Run(base::BindOnce(std::move(callback_), status_, response_));
    return false;
  }

  void Reenqueue(void* event_tag) override { NOTREACHED(); }

  bool CanStartRequest() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !callback_.is_null();
  }

  void OnRequestCanceled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_.Reset();
  }

  StartAndCreateReaderCallback create_reader_cb_;
  RunTaskCallback run_task_cb_;
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
      response_reader_;
  ResponseType response_;
  GrpcAsyncUnaryRpcCallback<ResponseType> callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncUnaryRequest);
};

// Creates a server streaming request.
// |rpc_function| is called once GrpcExecutor is about to send out the request.
// |callback| is called once the response is received from the server.
template <typename RequestType, typename ResponseType>
std::unique_ptr<GrpcAsyncUnaryRequest<ResponseType>>
CreateGrpcAsyncUnaryRequest(
    GrpcAsyncUnaryRpcFunction<RequestType, ResponseType> rpc_function,
    const RequestType& request,
    base::OnceCallback<void(const grpc::Status&, const ResponseType&)>
        callback) {
  // Cannot use make_unique because the constructor is private.
  std::unique_ptr<GrpcAsyncUnaryRequest<ResponseType>> grpc_request(
      new GrpcAsyncUnaryRequest<ResponseType>());
  auto create_reader_cb =
      base::BindOnce(std::move(rpc_function), grpc_request->context(), request);
  grpc_request->SetCallbacks(std::move(create_reader_cb), std::move(callback));
  return grpc_request;
}

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_
