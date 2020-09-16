// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/signaling/message_tracker.h"
#include "remoting/signaling/messaging_client.h"

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

class ProtobufHttpClient;
class MessageReceptionChannel;
class OAuthTokenGetter;
class RegistrationManager;
class ScopedProtobufHttpRequest;
class SignalingTracker;

// A class for sending and receiving messages via the FTL API.
class FtlMessagingClient final : public MessagingClient {
 public:
  // |signaling_tracker| is nullable.
  // Raw pointers must outlive |this|.
  FtlMessagingClient(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      RegistrationManager* registration_manager,
      SignalingTracker* signaling_tracker = nullptr);
  ~FtlMessagingClient() override;

  // MessagingClient implementations.
  std::unique_ptr<MessageCallbackSubscription> RegisterMessageCallback(
      const MessageCallback& callback) override;
  void PullMessages(DoneCallback on_done) override;
  void SendMessage(const std::string& destination,
                   const std::string& destination_registration_id,
                   const ftl::ChromotingMessage& message,
                   DoneCallback on_done) override;
  void StartReceivingMessages(base::OnceClosure on_ready,
                              DoneCallback on_closed) override;
  void StopReceivingMessages() override;
  bool IsReceivingMessages() const override;

 private:
  friend class FtlMessagingClientTest;

  FtlMessagingClient(std::unique_ptr<ProtobufHttpClient> client,
                     RegistrationManager* registration_manager,
                     std::unique_ptr<MessageReceptionChannel> channel);

  template <typename CallbackFunctor>
  void ExecuteRequest(const net::NetworkTrafficAnnotationTag& tag,
                      const std::string& path,
                      std::unique_ptr<google::protobuf::MessageLite> request,
                      CallbackFunctor callback_functor,
                      DoneCallback on_done);

  void OnPullMessagesResponse(
      DoneCallback on_done,
      const ProtobufHttpStatus& status,
      std::unique_ptr<ftl::PullMessagesResponse> response);

  void OnSendMessageResponse(DoneCallback on_done,
                             const ProtobufHttpStatus& status,
                             std::unique_ptr<ftl::InboxSendResponse> response);

  void AckMessages(const ftl::AckMessagesRequest& request,
                   DoneCallback on_done);

  void OnAckMessagesResponse(
      DoneCallback on_done,
      const ProtobufHttpStatus& status,
      std::unique_ptr<ftl::AckMessagesResponse> response);

  std::unique_ptr<ScopedProtobufHttpRequest> OpenReceiveMessagesStream(
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<
          void(std::unique_ptr<ftl::ReceiveMessagesResponse>)>& on_incoming_msg,
      base::OnceCallback<void(const ProtobufHttpStatus&)> on_channel_closed);

  void RunMessageCallbacks(const ftl::InboxMessage& message);

  void OnMessageReceived(const ftl::InboxMessage& message);

  std::unique_ptr<ProtobufHttpClient> client_;
  RegistrationManager* registration_manager_;
  std::unique_ptr<MessageReceptionChannel> reception_channel_;
  MessageCallbackList callback_list_;
  MessageTracker message_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FtlMessagingClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
