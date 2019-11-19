// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_
#define REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

class ScopedGrpcServerStream;

// Interface for starting or closing the server stream to receive messages from
// FTL backend.
class MessageReceptionChannel {
 public:
  using StreamOpener =
      base::RepeatingCallback<std::unique_ptr<ScopedGrpcServerStream>(
          base::OnceClosure on_channel_ready,
          const base::RepeatingCallback<void(
              const ftl::ReceiveMessagesResponse&)>& on_incoming_msg,
          base::OnceCallback<void(const grpc::Status&)> on_channel_closed)>;
  using MessageCallback =
      base::RepeatingCallback<void(const ftl::InboxMessage&)>;
  using DoneCallback = base::OnceCallback<void(const grpc::Status& status)>;

  MessageReceptionChannel() = default;
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

  // Closes the streaming channel.
  virtual void StopReceivingMessages() = 0;

  // Returns true if the streaming channel is open.
  virtual bool IsReceivingMessages() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceptionChannel);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGE_RECEPTION_CHANNEL_H_
