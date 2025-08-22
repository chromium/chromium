// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGE_CHANNEL_H_
#define REMOTING_SIGNALING_MESSAGE_CHANNEL_H_

#include <list>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace remoting {

class HttpStatus;
class MessageChannelStrategy;
class ScopedProtobufHttpRequest;
class SignalingTracker;

// Handles the lifetime and validity of a server-side stream.
// Use-case specific logic is provided by the MessageChannelStrategy passed into
// the c'tor. An optional |signaling_tracker| can be provided for zombie state
// detection.
class MessageChannel final {
 public:
  using DoneCallback = base::OnceCallback<void(const HttpStatus& status)>;

  // |signaling_tracker| is nullable.
  MessageChannel(std::unique_ptr<MessageChannelStrategy> strategy,
                 SignalingTracker* signaling_tracker);

  MessageChannel(const MessageChannel&) = delete;
  MessageChannel& operator=(const MessageChannel&) = delete;

  ~MessageChannel();

  // Opens a server streaming channel to receive messages.
  // |on_ready| is called once the stream is successfully started.
  // |on_closed| is called if the stream fails to start, in which case
  // |on_ready| will not be called, or when the stream is closed or dropped,
  // in which case it is called after |on_ready| is called.
  void StartReceivingMessages(base::OnceClosure on_ready,
                              DoneCallback on_closed);

  // Closes the streaming channel. |on_closed| callback will not be run.
  void StopReceivingMessages();

  // Returns true if the streaming channel is open.
  bool IsReceivingMessages() const;

  const net::BackoffEntry& GetReconnectRetryBackoffEntryForTesting() const;

 private:
  enum class State {
    // The stream is not active.
    STOPPED,

    // StartReceivingMessages() was called but the channel hasn't received a
    // message from the server yet.
    STARTING,

    // Stream has been started. This value represents both active connection as
    // well as when a connection is being re-established.
    STARTED,
  };

  void OnReceiveMessagesStreamReady();
  void OnReceiveMessagesStreamClosed(const HttpStatus& status);
  void OnChannelActive();

  void RunStreamReadyCallbacks();
  void RunStreamClosedCallbacks(const HttpStatus& status);
  void RetryStartReceivingMessagesWithBackoff();
  void RetryStartReceivingMessages();
  void StartReceivingMessagesInternal();
  void StopReceivingMessagesInternal();

  void BeginStreamTimers();
  void OnInactivityTimeout();

  std::unique_ptr<MessageChannelStrategy> strategy_;
  std::unique_ptr<ScopedProtobufHttpRequest> receive_messages_stream_;
  std::list<base::OnceClosure> stream_ready_callbacks_;
  std::list<DoneCallback> stream_closed_callbacks_;
  State state_ = State::STOPPED;
  net::BackoffEntry reconnect_retry_backoff_;
  base::OneShotTimer reconnect_retry_timer_;
  std::unique_ptr<base::DelayTimer> stream_inactivity_timer_;
  raw_ptr<SignalingTracker> signaling_tracker_;

  base::WeakPtrFactory<MessageChannel> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGE_CHANNEL_H_
