// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "remoting/proto/ftl/v1/ftl_services.grpc.pb.h"
#include "remoting/signaling/message_tracker.h"
#include "remoting/signaling/messaging_client.h"

namespace remoting {

class GrpcExecutor;
class MessageReceptionChannel;
class OAuthTokenGetter;
class RegistrationManager;
class ScopedGrpcServerStream;

// A class for sending and receiving messages via the FTL API.
class FtlMessagingClient final : public MessagingClient {
 public:
  // |token_getter| and |registration_manager| must outlive |this|.
  FtlMessagingClient(OAuthTokenGetter* token_getter,
                     RegistrationManager* registration_manager);
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
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  friend class FtlMessagingClientTest;

  FtlMessagingClient(std::unique_ptr<GrpcExecutor> executor,
                     RegistrationManager* registration_manager,
                     std::unique_ptr<MessageReceptionChannel> channel);

  void OnPullMessagesResponse(DoneCallback on_done,
                              const grpc::Status& status,
                              const ftl::PullMessagesResponse& response);

  void OnSendMessageResponse(DoneCallback on_done,
                             const grpc::Status& status,
                             const ftl::InboxSendResponse& response);

  void AckMessages(const ftl::AckMessagesRequest& request,
                   DoneCallback on_done);

  void OnAckMessagesResponse(DoneCallback on_done,
                             const grpc::Status& status,
                             const ftl::AckMessagesResponse& response);

  std::unique_ptr<ScopedGrpcServerStream> OpenReceiveMessagesStream(
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>&
          on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);

  void RunMessageCallbacks(const ftl::InboxMessage& message);

  void OnMessageReceived(const ftl::InboxMessage& message);

  std::unique_ptr<GrpcExecutor> executor_;
  RegistrationManager* registration_manager_;
  std::unique_ptr<Messaging::Stub> messaging_stub_;
  std::unique_ptr<MessageReceptionChannel> reception_channel_;
  MessageCallbackList callback_list_;
  MessageTracker message_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FtlMessagingClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
