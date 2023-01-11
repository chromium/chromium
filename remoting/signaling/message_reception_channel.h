// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_
#define REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

class ScopedProtobufHttpRequest;
class ProtobufHttpStatus;

// Interface for starting or closing the server stream to receive messages from
// FTL backend.
class MessageReceptionChannel {
 public:
  using StreamOpener =
      base::RepeatingCallback<std::unique_ptr<ScopedProtobufHttpRequest>(
          base::OnceClosure on_channel_ready,
          const base::RepeatingCallback<void(
              std::unique_ptr<ftl::ReceiveMessagesResponse>)>& on_incoming_msg,
          base::OnceCallback<void(const ProtobufHttpStatus&)>
              on_channel_closed)>;
  using MessageCallback =
      base::RepeatingCallback<void(const ftl::InboxMessage& message)>;
  using DoneCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status)>;

  MessageReceptionChannel() = default;

  MessageReceptionChannel(const MessageReceptionChannel&) = delete;
  MessageReceptionChannel& operator=(const MessageReceptionChannel&) = delete;

  virtual ~MessageReceptionChannel() = default;

  virtual void Initialize(const StreamOpener& stream_opener,
                          const MessageCallback& on_incoming_msg) = 0;

  // Opens a server streaming channel to the FTL API to enable message reception
  // over the fast path.
  // |on_ready| is called once the stream is successfully started.
  // |on_closed| is called if the stream fails to start, in which case
  // |on_ready| will not be called, or when the stream is closed or dropped,
  // in which case it is called after |on_ready| is called.
  virtual void StartReceivingMessages(base::OnceClosure on_ready,
                                      DoneCallback on_closed) = 0;

  // Closes the streaming channel. Note that |on_closed| callback will be
  // silently dropped.
  virtual void StopReceivingMessages() = 0;

  // Returns true if the streaming channel is open.
  virtual bool IsReceivingMessages() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_
