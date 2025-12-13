// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/message_channel.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/http_status.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/signaling/ftl_services_context.h"
#include "remoting/signaling/message_channel_strategy.h"
#include "remoting/signaling/signaling_tracker.h"

namespace remoting {

MessageChannel::MessageChannel(std::unique_ptr<MessageChannelStrategy> strategy,
                               SignalingTracker* signaling_tracker)
    : strategy_(std::move(strategy)),
      // TODO: joedow - Move backoff policy to a shared location.
      reconnect_retry_backoff_(&FtlServicesContext::GetBackoffPolicy()),
      signaling_tracker_(signaling_tracker) {
  strategy_->set_on_channel_active(base::BindRepeating(
      &MessageChannel::OnChannelActive, weak_factory_.GetWeakPtr()));
}

MessageChannel::~MessageChannel() = default;

void MessageChannel::StartReceivingMessages(base::OnceClosure on_ready,
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

void MessageChannel::StopReceivingMessages() {
  if (state_ == State::STOPPED) {
    return;
  }

  // Current stream callbacks shouldn't receive notification for future streams.
  stream_ready_callbacks_.clear();
  stream_closed_callbacks_.clear();
  StopReceivingMessagesInternal();
}

const net::BackoffEntry&
MessageChannel::GetReconnectRetryBackoffEntryForTesting() const {
  return reconnect_retry_backoff_;
}

void MessageChannel::OnReceiveMessagesStreamReady() {
  DCHECK_EQ(State::STARTING, state_);
  state_ = State::STARTED;

  if (signaling_tracker_) {
    signaling_tracker_->OnSignalingActive();
  }
  RunStreamReadyCallbacks();
  BeginStreamTimers();
}

void MessageChannel::OnReceiveMessagesStreamClosed(const HttpStatus& status) {
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
  if (status.error_code() == HttpStatus::Code::ABORTED ||
      status.error_code() == HttpStatus::Code::UNAVAILABLE ||
      status.error_code() == HttpStatus::Code::NETWORK_ERROR) {
    // These are 'soft' connection errors that should be retried.
    // Other errors should be ignored.
    RetryStartReceivingMessagesWithBackoff();
    return;
  }

  stream_ready_callbacks_.clear();
  StopReceivingMessagesInternal();
  RunStreamClosedCallbacks(status);
}

void MessageChannel::RunStreamReadyCallbacks() {
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

void MessageChannel::RunStreamClosedCallbacks(const HttpStatus& status) {
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

void MessageChannel::RetryStartReceivingMessagesWithBackoff() {
  VLOG(1) << "RetryStartReceivingMessages will be called with backoff: "
          << reconnect_retry_backoff_.GetTimeUntilRelease();
  reconnect_retry_timer_.Start(
      FROM_HERE, reconnect_retry_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&MessageChannel::RetryStartReceivingMessages,
                     base::Unretained(this)));
}

void MessageChannel::RetryStartReceivingMessages() {
  VLOG(1) << "RetryStartReceivingMessages called";
  StopReceivingMessagesInternal();
  StartReceivingMessagesInternal();
}

void MessageChannel::StartReceivingMessagesInternal() {
  DCHECK_EQ(State::STOPPED, state_);
  state_ = State::STARTING;
  receive_messages_stream_ = strategy_->CreateChannel(
      base::BindOnce(&MessageChannel::OnReceiveMessagesStreamReady,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&MessageChannel::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void MessageChannel::StopReceivingMessagesInternal() {
  DCHECK_NE(State::STOPPED, state_);

  state_ = State::STOPPED;
  receive_messages_stream_.reset();
  reconnect_retry_timer_.Stop();
  stream_inactivity_timer_.reset();
}

bool MessageChannel::IsReceivingMessages() const {
  return receive_messages_stream_.get() != nullptr;
}

void MessageChannel::BeginStreamTimers() {
  reconnect_retry_backoff_.Reset();
  stream_inactivity_timer_ = std::make_unique<base::DelayTimer>(
      FROM_HERE, strategy_->GetInactivityTimeout(), this,
      &MessageChannel::OnInactivityTimeout);
  stream_inactivity_timer_->Reset();
}

void MessageChannel::OnInactivityTimeout() {
  LOG(WARNING) << "Timed out waiting for message from server.";
  reconnect_retry_backoff_.InformOfRequest(false);
  RetryStartReceivingMessagesWithBackoff();
}

void MessageChannel::OnChannelActive() {
  stream_inactivity_timer_->Reset();
  if (signaling_tracker_) {
    signaling_tracker_->OnSignalingActive();
  }
}

}  // namespace remoting
