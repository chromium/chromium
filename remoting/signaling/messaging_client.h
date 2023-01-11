// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_MESSAGING_CLIENT_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

class ProtobufHttpStatus;

// An interface to send messages and receive messages from FTL messaging
// service.
class MessagingClient {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const ftl::Id& sender_id,
                                   const std::string& sender_registration_id,
                                   const ftl::ChromotingMessage& message)>;
  using MessageCallbackList = base::RepeatingCallbackList<
      void(const ftl::Id&, const std::string&, const ftl::ChromotingMessage&)>;
  using DoneCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status)>;

  virtual ~MessagingClient() = default;

  // Registers a callback which is run for each new message received. Simply
  // delete the returned subscription object to unregister. The subscription
  // object must be deleted before |this| is deleted.
  virtual base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback) = 0;

  virtual void SendMessage(const std::string& destination,
                           const std::string& destination_registration_id,
                           const ftl::ChromotingMessage& message,
                           DoneCallback on_done) = 0;

  // Opens a stream to continuously receive new messages from the server and
  // calls the registered MessageCallback once a new message is received.
  // |on_ready| is called once the stream is successfully started.
  // |on_closed| is called if the stream fails to start, in which case
  // |on_ready| will not be called, or when the stream is closed or dropped,
  // in which case it is called after |on_ready| is called.
  virtual void StartReceivingMessages(base::OnceClosure on_ready,
                                      DoneCallback on_closed) = 0;

  // Stops the stream for continuously receiving new messages. Note that
  // |on_closed| callback will be silently dropped.
  virtual void StopReceivingMessages() = 0;

  // Returns true if the streaming channel is open.
  virtual bool IsReceivingMessages() const = 0;

 protected:
  MessagingClient() = default;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGING_CLIENT_H_
