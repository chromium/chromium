// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/grpc_support/grpc_async_request.h"
#include "remoting/base/grpc_support/scoped_grpc_server_stream.h"
#include "third_party/grpc/src/include/grpcpp/support/async_stream.h"

namespace remoting {

template <typename RequestType, typename ResponseType>
using GrpcAsyncServerStreamingRpcFunction =
    base::OnceCallback<std::unique_ptr<grpc::ClientAsyncReader<ResponseType>>(
        grpc::ClientContext*,
        const RequestType&,
        grpc_impl::CompletionQueue*,
        void*)>;

// GrpcAsyncRequest implementation for server streaming call. The object is
// first enqueued for starting the stream, then kept being re-enqueued to
// receive a new message, until it's canceled by calling CancelRequest().
class GrpcAsyncServerStreamingRequestBase : public GrpcAsyncRequest {
 public:
  GrpcAsyncServerStreamingRequestBase(
      base::OnceClosure on_channel_ready,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      std::unique_ptr<ScopedGrpcServerStream>* scoped_stream);
  ~GrpcAsyncServerStreamingRequestBase() override;

  // Sets the deadline of receiving initial metadata (marker of stream being
  // started). The request will be closed with a DEADLINE_EXCEEDED error if
  // initial metadata has not been received after |deadline|.
  //
  // Note that this is different than setting deadline on the client context,
  // which will close the stream if the server doesn't close it after
  // |deadline|.
  //
  // The default value is 30s after request is started.
  void set_initial_metadata_deadline(base::Time deadline) {
    initial_metadata_deadline_ = deadline;
  }

 protected:
  enum class State {
    STARTING,
    PENDING_INITIAL_METADATA,
    STREAMING,

    // Server has closed the stream and we are getting back the reason.
    FINISHING,

    CLOSED,
  };

  void set_run_task_callback(const RunTaskCallback& callback) {
    run_task_callback_ = callback;
  }

  // Schedules a task with |run_task_callback_|. Drops it if the scoped stream
  // has been deleted right before it is being executed.
  void RunTask(base::OnceClosure task);

  void StartInitialMetadataTimer();

  virtual void ReadInitialMetadata(void* event_tag) = 0;
  virtual void ResolveIncomingMessage() = 0;
  virtual void WaitForIncomingMessage(void* event_tag) = 0;
  virtual void FinishStream(void* event_tag) = 0;

 private:
  // GrpcAsyncRequest implementations.
  bool OnDequeue(bool operation_succeeded) override;
  void Reenqueue(void* event_tag) override;
  void OnRequestCanceled() override;
  bool CanStartRequest() const override;
  void ResolveChannelReady();
  void ResolveChannelClosed();
  void OnInitialMetadataTimeout();

  base::OnceClosure on_channel_ready_;
  base::OnceCallback<void(const grpc::Status&)> on_channel_closed_;
  State state_ = State::STARTING;
  base::Time initial_metadata_deadline_;
  base::OneShotTimer initial_metadata_timer_;

  RunTaskCallback run_task_callback_;
  base::WeakPtr<ScopedGrpcServerStream> scoped_stream_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GrpcAsyncServerStreamingRequestBase> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingRequestBase);
};

template <typename ResponseType>
class GrpcAsyncServerStreamingRequest
    : public GrpcAsyncServerStreamingRequestBase {
 public:
  using OnIncomingMessageCallback =
      base::RepeatingCallback<void(const ResponseType&)>;
  using StartAndCreateReaderCallback =
      base::OnceCallback<std::unique_ptr<grpc::ClientAsyncReader<ResponseType>>(
          grpc_impl::CompletionQueue* cq,
          void* event_tag)>;

  ~GrpcAsyncServerStreamingRequest() override = default;

 private:
  template <typename Req, typename Res>
  friend std::unique_ptr<GrpcAsyncServerStreamingRequest<Res>>
  CreateGrpcAsyncServerStreamingRequest(
      GrpcAsyncServerStreamingRpcFunction<Req, Res> rpc_function,
      const Req& request,
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<void(const Res&)>& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      std::unique_ptr<ScopedGrpcServerStream>* scoped_stream);

  GrpcAsyncServerStreamingRequest(
      base::OnceClosure on_channel_ready,
      const OnIncomingMessageCallback& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      std::unique_ptr<ScopedGrpcServerStream>* scoped_stream)
      : GrpcAsyncServerStreamingRequestBase(std::move(on_channel_ready),
                                            std::move(on_channel_closed),
                                            scoped_stream) {
    on_incoming_msg_ = on_incoming_msg;
  }

  void SetCreateReaderCallback(StartAndCreateReaderCallback callback) {
    DCHECK(!create_reader_callback_);
    DCHECK(callback);
    create_reader_callback_ = std::move(callback);
  }

  // GrpcAsyncRequest implementations
  void Start(const RunTaskCallback& run_task_cb,
             grpc_impl::CompletionQueue* cq,
             void* event_tag) override {
    reader_ = std::move(create_reader_callback_).Run(cq, event_tag);
    set_run_task_callback(run_task_cb);
    StartInitialMetadataTimer();
  }

  // GrpcAsyncServerStreamingRequestBase implementations.
  void ReadInitialMetadata(void* event_tag) override {
    DCHECK(reader_);
    reader_->ReadInitialMetadata(event_tag);
  }

  void ResolveIncomingMessage() override {
    RunTask(base::BindOnce(on_incoming_msg_, response_));
  }

  void WaitForIncomingMessage(void* event_tag) override {
    DCHECK(reader_);
    reader_->Read(&response_, event_tag);
  }

  void FinishStream(void* event_tag) override {
    DCHECK(reader_);
    reader_->Finish(&status_, event_tag);
  }

  StartAndCreateReaderCallback create_reader_callback_;
  ResponseType response_;
  std::unique_ptr<grpc::ClientAsyncReader<ResponseType>> reader_;
  OnIncomingMessageCallback on_incoming_msg_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingRequest);
};

// Creates a server streaming request.
// |rpc_function| is called once GrpcExecutor is about to send out the request.
// |on_incoming_msg| is called once a message is streamed from the server.
// |on_channel_closed| is called once the channel is closed remotely by the
// server.
// |scoped_stream| is set with an object which upon destruction will cancel the
// stream.
template <typename RequestType, typename ResponseType>
std::unique_ptr<GrpcAsyncServerStreamingRequest<ResponseType>>
CreateGrpcAsyncServerStreamingRequest(
    GrpcAsyncServerStreamingRpcFunction<RequestType, ResponseType> rpc_function,
    const RequestType& request,
    base::OnceClosure on_channel_ready,
    const base::RepeatingCallback<void(const ResponseType&)>& on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
    std::unique_ptr<ScopedGrpcServerStream>* scoped_stream) {
  // Cannot use make_unique because the constructor is private.
  std::unique_ptr<GrpcAsyncServerStreamingRequest<ResponseType>> grpc_request(
      new GrpcAsyncServerStreamingRequest<ResponseType>(
          std::move(on_channel_ready), on_incoming_msg,
          std::move(on_channel_closed), scoped_stream));
  grpc_request->SetCreateReaderCallback(base::BindOnce(
      std::move(rpc_function), grpc_request->context(), request));
  return grpc_request;
}

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
