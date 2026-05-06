// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/http_status.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/message_tracker.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class FtlMessageChannelStrategy;
class MessageChannel;
class ProtobufHttpClient;
class OAuthTokenGetter;
class RegistrationManager;
class ScopedProtobufHttpRequest;
class SignalingAddress;
class SignalingTracker;

// A class for sending and receiving messages via the FTL API.
class FtlMessagingClient {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const SignalingAddress& sender_address,
                                   const ftl::ChromotingMessage& message)>;
  using MessageCallbackList =
      base::RepeatingCallbackList<void(const SignalingAddress&,
                                       const ftl::ChromotingMessage&)>;
  using DoneCallback =
      base::OnceCallback<void(const remoting::HttpStatus& status)>;

  // |signaling_tracker| is nullable.
  // Raw pointers must outlive |this|.
  FtlMessagingClient(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      RegistrationManager* registration_manager,
      SignalingTracker* signaling_tracker = nullptr);

  FtlMessagingClient(const FtlMessagingClient&) = delete;
  FtlMessagingClient& operator=(const FtlMessagingClient&) = delete;

  virtual ~FtlMessagingClient();

  // Registers a callback which is run for each new message received. Simply
  // delete the returned subscription object to unregister. The subscription
  // object must be deleted before |this| is deleted.
  virtual base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback);

  // Sends |message| to |destination_address| and then calls |on_done| with the
  // result of the operation. If non-null, |retry_policy| controls retries.
  virtual void SendMessage(
      const SignalingAddress& destination_address,
      ftl::ChromotingMessage&& message,
      DoneCallback on_done,
      scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> retry_policy);

  // Helper function that calls SendMessage() with retries disabled.
  virtual void SendMessage(const SignalingAddress& destination_address,
                           ftl::ChromotingMessage&& message,
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

 private:
  friend class FtlMessagingClientTest;

  FtlMessagingClient(std::unique_ptr<ProtobufHttpClient> client,
                     RegistrationManager* registration_manager,
                     SignalingTracker* signaling_tracker,
                     std::unique_ptr<FtlMessageChannelStrategy> strategy);

  template <typename CallbackFunctor>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& tag,
      const std::string& path,
      std::unique_ptr<google::protobuf::MessageLite> request,
      CallbackFunctor callback_functor,
      DoneCallback on_done,
      scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> retry_policy);

  void OnSendMessageResponse(DoneCallback on_done,
                             const remoting::HttpStatus& status,
                             std::unique_ptr<ftl::InboxSendResponse> response);

  void BatchAckMessages(const ftl::BatchAckMessagesRequest& request,
                        DoneCallback on_done);

  void OnBatchAckMessagesResponse(
      DoneCallback on_done,
      const remoting::HttpStatus& status,
      std::unique_ptr<ftl::BatchAckMessagesResponse> response);

  std::unique_ptr<ScopedProtobufHttpRequest> OpenReceiveMessagesStream(
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<
          void(std::unique_ptr<ftl::ReceiveMessagesResponse>)>& on_incoming_msg,
      base::OnceCallback<void(const remoting::HttpStatus&)> on_channel_closed);

  void RunMessageCallbacks(const ftl::InboxMessage& message);

  void OnMessageReceived(const ftl::InboxMessage& message);

  std::unique_ptr<ProtobufHttpClient> client_;
  raw_ptr<RegistrationManager> registration_manager_;
  std::unique_ptr<MessageChannel> message_channel_;
  MessageCallbackList callback_list_;
  MessageTracker message_tracker_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
