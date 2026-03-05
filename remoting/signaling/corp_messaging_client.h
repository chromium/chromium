// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_CORP_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_CORP_MESSAGING_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/internal_headers.h"
#include "remoting/signaling/corp_message_channel_strategy.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
class ClientCertStore;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class CorpMessageChannelStrategy;
class HttpStatus;
class MessageChannel;
class ProtobufHttpClient;
class ScopedProtobufHttpRequest;
class SignalingAddress;

// A class for sending and receiving messages via the Corp messaging API.
class CorpMessagingClient {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const SignalingAddress& sender_address,
                                   const internal::PeerMessageStruct& message)>;
  using MessageCallbackList =
      base::RepeatingCallbackList<void(const SignalingAddress&,
                                       const internal::PeerMessageStruct&)>;
  using DoneCallback = base::OnceCallback<void(const HttpStatus& status)>;
  using SignalingAddressChangedCallback =
      CorpMessageChannelStrategy::SignalingAddressChangedCallback;

  CorpMessagingClient(
      const std::string& username,
      const std::string& public_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<net::ClientCertStore> client_cert_store,
      const SignalingAddressChangedCallback& on_signaling_address_changed);

  CorpMessagingClient(const CorpMessagingClient&) = delete;
  CorpMessagingClient& operator=(const CorpMessagingClient&) = delete;

  virtual ~CorpMessagingClient();

  // Registers a callback which is run for each new message received. Simply
  // delete the returned subscription object to unregister. The subscription
  // object must be deleted before |this| is deleted.
  virtual base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback);

  // Sends |message| to |destination_address| and then calls |on_done| with the
  // result of the operation.
  virtual void SendMessage(const SignalingAddress& destination_address,
                           internal::PeerMessageStruct&& message,
                           DoneCallback on_done);

  // Opens a stream to continuously receive new messages from the server and
  // calls the registered MessageCallback once a new message is received.
  // |on_ready| is called once the stream is successfully started.
  // |on_closed| is called if the stream fails to start, in which case
  // |on_ready| will not be called, or when the stream is closed or dropped,
  // in which case it is called after |on_ready| is called.
  virtual void StartReceivingMessages(base::OnceClosure on_ready,
                                      DoneCallback on_closed);

  // Stops the stream for continuously receiving new messages. Note that
  // |on_closed| callback will be silently dropped.
  virtual void StopReceivingMessages();

  // Returns true if the streaming channel is open.
  virtual bool IsReceivingMessages() const;

 protected:
  CorpMessagingClient();

 private:
  template <typename CallbackFunctor>
  void ExecuteRequest(const net::NetworkTrafficAnnotationTag& tag,
                      const std::string& path,
                      bool enable_retries,
                      std::unique_ptr<google::protobuf::MessageLite> request,
                      CallbackFunctor callback_functor,
                      DoneCallback on_done);

  void OnSendMessageResponse(
      DoneCallback on_done,
      const HttpStatus& status,
      std::unique_ptr<internal::HostSendMessageResponse> response);

  std::unique_ptr<ScopedProtobufHttpRequest> OpenReceiveMessagesStream(
      base::OnceClosure on_channel_ready,
      const CorpMessageChannelStrategy::MessageReceivedCallback& on_message,
      DoneCallback on_channel_closed);

  void OnMessageReceived(const internal::PeerMessageStruct& message);

  std::string username_;
  std::string public_key_;
  std::unique_ptr<ProtobufHttpClient> client_;
  std::unique_ptr<MessageChannel> message_channel_;
  MessageCallbackList message_callback_list_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_CORP_MESSAGING_CLIENT_H_
