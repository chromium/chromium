// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "remoting/base/grpc_support/grpc_async_server_streaming_request.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/grpc_support/grpc_executor.h"
#include "remoting/signaling/ftl_grpc_context.h"
#include "remoting/signaling/ftl_message_reception_channel.h"
#include "remoting/signaling/registration_manager.h"

namespace remoting {

namespace {

void AddMessageToAckRequest(const ftl::InboxMessage& message,
                            ftl::AckMessagesRequest* request) {
  ftl::ReceiverMessage* receiver_message = request->add_messages();
  receiver_message->set_message_id(message.message_id());
  receiver_message->set_allocated_receiver_id(
      new ftl::Id(message.receiver_id()));
}

constexpr base::TimeDelta kInboxMessageTtl = base::TimeDelta::FromMinutes(1);

}  // namespace

FtlMessagingClient::FtlMessagingClient(
    OAuthTokenGetter* token_getter,
    RegistrationManager* registration_manager)
    : FtlMessagingClient(
          std::make_unique<GrpcAuthenticatedExecutor>(token_getter),
          registration_manager,
          std::make_unique<FtlMessageReceptionChannel>()) {}

FtlMessagingClient::FtlMessagingClient(
    std::unique_ptr<GrpcExecutor> executor,
    RegistrationManager* registration_manager,
    std::unique_ptr<MessageReceptionChannel> channel) {
  DCHECK(executor);
  DCHECK(registration_manager);
  DCHECK(channel);
  executor_ = std::move(executor);
  registration_manager_ = registration_manager;
  messaging_stub_ = Messaging::NewStub(FtlGrpcContext::CreateChannel());
  reception_channel_ = std::move(channel);
  reception_channel_->Initialize(
      base::BindRepeating(&FtlMessagingClient::OpenReceiveMessagesStream,
                          base::Unretained(this)),
      base::BindRepeating(&FtlMessagingClient::OnMessageReceived,
                          base::Unretained(this)));
}

FtlMessagingClient::~FtlMessagingClient() = default;

std::unique_ptr<FtlMessagingClient::MessageCallbackSubscription>
FtlMessagingClient::RegisterMessageCallback(const MessageCallback& callback) {
  return callback_list_.Add(callback);
}

void FtlMessagingClient::PullMessages(DoneCallback on_done) {
  ftl::PullMessagesRequest request;
  *request.mutable_header() = FtlGrpcContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&Messaging::Stub::AsyncPullMessages,
                     base::Unretained(messaging_stub_.get())),
      request,
      base::BindOnce(&FtlMessagingClient::OnPullMessagesResponse,
                     base::Unretained(this), std::move(on_done)));
  FtlGrpcContext::FillClientContext(grpc_request->context());
  executor_->ExecuteRpc(std::move(grpc_request));
}

void FtlMessagingClient::SendMessage(
    const std::string& destination,
    const std::string& destination_registration_id,
    const ftl::ChromotingMessage& message,
    DoneCallback on_done) {
  ftl::InboxSendRequest request;
  *request.mutable_header() = FtlGrpcContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  request.set_time_to_live(kInboxMessageTtl.InMicroseconds());
  // TODO(yuweih): See if we need to set requester_id
  *request.mutable_dest_id() = FtlGrpcContext::CreateIdFromString(destination);

  std::string serialized_message;
  bool succeeded = message.SerializeToString(&serialized_message);
  DCHECK(succeeded);

  request.mutable_message()->set_message(serialized_message);
  request.mutable_message()->set_message_id(base::GenerateGUID());
  request.mutable_message()->set_message_type(
      ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  request.mutable_message()->set_message_class(
      ftl::InboxMessage_MessageClass_STATUS);
  if (!destination_registration_id.empty()) {
    request.add_dest_registration_ids(destination_registration_id);
  }

  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&Messaging::Stub::AsyncSendMessage,
                     base::Unretained(messaging_stub_.get())),
      request,
      base::BindOnce(&FtlMessagingClient::OnSendMessageResponse,
                     base::Unretained(this), std::move(on_done)));
  FtlGrpcContext::FillClientContext(grpc_request->context());
  executor_->ExecuteRpc(std::move(grpc_request));
}

void FtlMessagingClient::StartReceivingMessages(base::OnceClosure on_ready,
                                                DoneCallback on_closed) {
  reception_channel_->StartReceivingMessages(std::move(on_ready),
                                             std::move(on_closed));
}

void FtlMessagingClient::StopReceivingMessages() {
  reception_channel_->StopReceivingMessages();
}

bool FtlMessagingClient::IsReceivingMessages() const {
  return reception_channel_->IsReceivingMessages();
}

void FtlMessagingClient::OnPullMessagesResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::PullMessagesResponse& response) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to pull messages. "
               << "Error code: " << status.error_code()
               << ", message: " << status.error_message();
    std::move(on_done).Run(status);
    return;
  }

  ftl::AckMessagesRequest ack_request;
  *ack_request.mutable_header() = FtlGrpcContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  for (const auto& message : response.messages()) {
    RunMessageCallbacks(message);
    AddMessageToAckRequest(message, &ack_request);
  }

  if (ack_request.messages_size() == 0) {
    LOG(WARNING) << "No new message is received.";
    std::move(on_done).Run(status);
    return;
  }

  VLOG(1) << "Acking " << ack_request.messages_size() << " messages";

  AckMessages(ack_request, std::move(on_done));
}

void FtlMessagingClient::OnSendMessageResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::InboxSendResponse& response) {
  std::move(on_done).Run(status);
}

void FtlMessagingClient::AckMessages(const ftl::AckMessagesRequest& request,
                                     DoneCallback on_done) {
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&Messaging::Stub::AsyncAckMessages,
                     base::Unretained(messaging_stub_.get())),
      request,
      base::BindOnce(&FtlMessagingClient::OnAckMessagesResponse,
                     base::Unretained(this), std::move(on_done)));
  FtlGrpcContext::FillClientContext(grpc_request->context());
  executor_->ExecuteRpc(std::move(grpc_request));
}

void FtlMessagingClient::OnAckMessagesResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::AckMessagesResponse& response) {
  // TODO(yuweih): Handle failure.
  std::move(on_done).Run(status);
}

std::unique_ptr<ScopedGrpcServerStream>
FtlMessagingClient::OpenReceiveMessagesStream(
    base::OnceClosure on_channel_ready,
    const base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>&
        on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed) {
  ftl::ReceiveMessagesRequest request;
  *request.mutable_header() = FtlGrpcContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  std::unique_ptr<ScopedGrpcServerStream> stream;
  auto grpc_request = CreateGrpcAsyncServerStreamingRequest(
      base::BindOnce(&Messaging::Stub::AsyncReceiveMessages,
                     base::Unretained(messaging_stub_.get())),
      request, std::move(on_channel_ready), on_incoming_msg,
      std::move(on_channel_closed), &stream);
  FtlGrpcContext::FillClientContext(grpc_request->context());
  executor_->ExecuteRpc(std::move(grpc_request));
  return stream;
}

void FtlMessagingClient::RunMessageCallbacks(const ftl::InboxMessage& message) {
  if (message_tracker_.IsIdTracked(message.message_id())) {
    LOG(WARNING) << "Found message with duplicated message ID: "
                 << message.message_id();
    return;
  }
  message_tracker_.TrackId(message.message_id());

  if (message.sender_id().type() != ftl::IdType_Type_SYSTEM &&
      message.sender_registration_id().empty()) {
    LOG(WARNING) << "Ignored peer message with no sender registration ID.";
    return;
  }

  if (message.message_type() !=
      ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE) {
    LOG(WARNING) << "Received message with unknown type: "
                 << message.message_type()
                 << ", sender: " << message.sender_id().id();
    return;
  }

  ftl::ChromotingMessage chromoting_message;
  chromoting_message.ParseFromString(message.message());
  callback_list_.Notify(message.sender_id(), message.sender_registration_id(),
                        chromoting_message);
}

void FtlMessagingClient::OnMessageReceived(const ftl::InboxMessage& message) {
  RunMessageCallbacks(message);
  ftl::AckMessagesRequest ack_request;
  *ack_request.mutable_header() = FtlGrpcContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  AddMessageToAckRequest(message, &ack_request);
  AckMessages(ack_request, base::DoNothing());
}

}  // namespace remoting
