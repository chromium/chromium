// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_message_reception_channel.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_services_context.h"

namespace remoting {

constexpr base::TimeDelta FtlMessageReceptionChannel::kPongTimeout;

FtlMessageReceptionChannel::FtlMessageReceptionChannel(
    SignalingTracker* signaling_tracker)
    : reconnect_retry_backoff_(&FtlServicesContext::GetBackoffPolicy()),
      signaling_tracker_(signaling_tracker) {}

FtlMessageReceptionChannel::~FtlMessageReceptionChannel() = default;

void FtlMessageReceptionChannel::Initialize(
    const StreamOpener& stream_opener,
    const MessageCallback& on_incoming_msg) {
  DCHECK(stream_opener);
  DCHECK(on_incoming_msg);
  DCHECK(!stream_opener_);
  DCHECK(!on_incoming_msg_);
  stream_opener_ = stream_opener;
  on_incoming_msg_ = on_incoming_msg;
}

void FtlMessageReceptionChannel::StartReceivingMessages(
    base::OnceClosure on_ready,
    DoneCallback on_closed) {
  stream_closed_callbacks_.push_back(std::move(on_closed));
  if (state_ == State::STARTED) {
    std::move(on_ready).Run();
    return;
  }
  stream_ready_callbacks_.push_back(std::move(on_ready));
  if (state_ == State::STARTING) {
    return;
  }

  state_ = State::STARTING;
  RetryStartReceivingMessagesWithBackoff();
}

void FtlMessageReceptionChannel::StopReceivingMessages() {
  if (state_ == State::STOPPED) {
    return;
  }

  // Current stream callbacks shouldn't receive notification for future streams.
  stream_ready_callbacks_.clear();
  stream_closed_callbacks_.clear();
  StopReceivingMessagesInternal();
}

const net::BackoffEntry&
FtlMessageReceptionChannel::GetReconnectRetryBackoffEntryForTesting() const {
  return reconnect_retry_backoff_;
}

void FtlMessageReceptionChannel::OnReceiveMessagesStreamReady() {
  DCHECK_EQ(State::STARTING, state_);
  state_ = State::STARTED;
  if (signaling_tracker_) {
    signaling_tracker_->OnSignalingActive();
  }
  RunStreamReadyCallbacks();
  BeginStreamTimers();
}

void FtlMessageReceptionChannel::OnReceiveMessagesStreamClosed(
    const ProtobufHttpStatus& status) {
  if (state_ == State::STOPPED) {
    // Previously closed by the caller.
    return;
  }
  if (status.ok()) {
    // The backend closes the stream. This is not an error so we restart it
    // without backoff.
    VLOG(1) << "Stream has been closed by the server. Reconnecting...";
    reconnect_retry_backoff_.Reset();
    RetryStartReceivingMessages();
    return;
  }

  reconnect_retry_backoff_.InformOfRequest(false);
  if (status.error_code() == ProtobufHttpStatus::Code::ABORTED ||
      status.error_code() == ProtobufHttpStatus::Code::UNAVAILABLE) {
    // These are 'soft' connection errors that should be retried.
    // Other errors should be ignored.
    RetryStartReceivingMessagesWithBackoff();
    return;
  }
  stream_ready_callbacks_.clear();
  StopReceivingMessagesInternal();
  RunStreamClosedCallbacks(status);
}

void FtlMessageReceptionChannel::OnMessageReceived(
    std::unique_ptr<ftl::ReceiveMessagesResponse> response) {
  switch (response->body_case()) {
    case ftl::ReceiveMessagesResponse::BodyCase::kInboxMessage: {
      VLOG(1) << "Received message";
      on_incoming_msg_.Run(response->inbox_message());
      break;
    }
    case ftl::ReceiveMessagesResponse::BodyCase::kPong:
      VLOG(1) << "Received pong";
      stream_pong_timer_->Reset();
      if (signaling_tracker_) {
        signaling_tracker_->OnSignalingActive();
      }
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kStartOfBatch:
      VLOG(1) << "Received start of batch";
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kEndOfBatch:
      VLOG(1) << "Received end of batch";
      break;
    default:
      LOG(WARNING) << "Received unknown message type: "
                   << response->body_case();
      break;
  }
}

void FtlMessageReceptionChannel::RunStreamReadyCallbacks() {
  if (stream_ready_callbacks_.empty()) {
    return;
  }

  // The swap is to make StartReceivingMessages() reentrant.
  std::list<base::OnceClosure> callbacks;
  callbacks.swap(stream_ready_callbacks_);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

void FtlMessageReceptionChannel::RunStreamClosedCallbacks(
    const ProtobufHttpStatus& status) {
  if (stream_closed_callbacks_.empty()) {
    return;
  }

  // The swap is to make StartReceivingMessages() reentrant.
  std::list<DoneCallback> callbacks;
  callbacks.swap(stream_closed_callbacks_);
  for (DoneCallback& callback : callbacks) {
    std::move(callback).Run(status);
  }
}

void FtlMessageReceptionChannel::RetryStartReceivingMessagesWithBackoff() {
  VLOG(1) << "RetryStartReceivingMessages will be called with backoff: "
          << reconnect_retry_backoff_.GetTimeUntilRelease();
  reconnect_retry_timer_.Start(
      FROM_HERE, reconnect_retry_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&FtlMessageReceptionChannel::RetryStartReceivingMessages,
                     base::Unretained(this)));
}

void FtlMessageReceptionChannel::RetryStartReceivingMessages() {
  VLOG(1) << "RetryStartReceivingMessages called";
  StopReceivingMessagesInternal();
  StartReceivingMessagesInternal();
}

void FtlMessageReceptionChannel::StartReceivingMessagesInternal() {
  DCHECK_EQ(State::STOPPED, state_);
  state_ = State::STARTING;
  receive_messages_stream_ = stream_opener_.Run(
      base::BindOnce(&FtlMessageReceptionChannel::OnReceiveMessagesStreamReady,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FtlMessageReceptionChannel::OnMessageReceived,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&FtlMessageReceptionChannel::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void FtlMessageReceptionChannel::StopReceivingMessagesInternal() {
  DCHECK_NE(State::STOPPED, state_);
  state_ = State::STOPPED;
  receive_messages_stream_.reset();
  reconnect_retry_timer_.Stop();
  stream_pong_timer_.reset();
}

bool FtlMessageReceptionChannel::IsReceivingMessages() const {
  return receive_messages_stream_.get() != nullptr;
}

void FtlMessageReceptionChannel::BeginStreamTimers() {
  reconnect_retry_backoff_.Reset();
  stream_pong_timer_ = std::make_unique<base::DelayTimer>(
      FROM_HERE, kPongTimeout, this,
      &FtlMessageReceptionChannel::OnPongTimeout);
  stream_pong_timer_->Reset();
}

void FtlMessageReceptionChannel::OnPongTimeout() {
  LOG(WARNING) << "Timed out waiting for PONG message from server.";
  reconnect_retry_backoff_.InformOfRequest(false);
  RetryStartReceivingMessagesWithBackoff();
}

}  // namespace remoting
