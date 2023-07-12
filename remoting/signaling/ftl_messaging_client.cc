// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_stream_request.h"
#include "remoting/signaling/ftl_message_reception_channel.h"
#include "remoting/signaling/ftl_services_context.h"
#include "remoting/signaling/registration_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

constexpr char kBatchAckMessagesPath[] = "/v1/message:batchAckMessages";
constexpr char kReceiveMessagesPath[] = "/v1/messages:receive";
constexpr char kSendMessagePath[] = "/v1/message:send";

constexpr net::NetworkTrafficAnnotationTag kAckMessagesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ftl_messaging_client_ack_messages",
                                        R"(
    semantics {
      sender: "Chrome Remote Desktop"
      description:
        "Acknowledges the receipt of a signaling message from the Chrome "
        "Remote Desktop backend."
      trigger:
        "Initiating a Chrome Remote Desktop connection."
      user_data {
        type: CREDENTIALS
      }
      data:
        "User's auth code and message ID for the message to be acknowledged."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts { email: "garykac@chromium.org" }
        contacts { email: "jamiewalch@chromium.org" }
        contacts { email: "joedow@chromium.org" }
        contacts { email: "lambroslambrou@chromium.org" }
        contacts { email: "rkjnsn@chromium.org" }
        contacts { email: "yuweih@chromium.org" }
      }
      last_reviewed: "2023-07-07"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request cannot be stopped in settings, but will not be sent "
        "if the user does not use Chrome Remote Desktop."
      chrome_policy {
        RemoteAccessHostAllowRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowRemoteSupportConnections: false
        }
        RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
        }
      }
    })");

constexpr net::NetworkTrafficAnnotationTag kReceiveMessagesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ftl_messaging_client_receive_messages",
                                        R"(
    semantics {
      sender: "Chrome Remote Desktop"
      description:
        "Retrieves signaling messages from the Chrome Remote Desktop peer "
        "(either a Chrome Remote Desktop host or client) via the Chrome Remote "
        "Desktop backend."
      trigger:
        "Initiating a Chrome Remote Desktop connection."
      user_data {
        type: CREDENTIALS
      }
      data:
        "User's auth code and registration ID for retrieving messages."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts { email: "garykac@chromium.org" }
        contacts { email: "jamiewalch@chromium.org" }
        contacts { email: "joedow@chromium.org" }
        contacts { email: "lambroslambrou@chromium.org" }
        contacts { email: "rkjnsn@chromium.org" }
        contacts { email: "yuweih@chromium.org" }
      }
      last_reviewed: "2023-07-07"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request cannot be stopped in settings, but will not be sent "
        "if the user does not use Chrome Remote Desktop."
      chrome_policy {
        RemoteAccessHostAllowRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowRemoteSupportConnections: false
        }
        RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
        }
      }
    })");

constexpr net::NetworkTrafficAnnotationTag kSendMessageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ftl_messaging_client_send_messages",
                                        R"(
    semantics {
      sender: "Chrome Remote Desktop"
      description:
        "Sends signaling messages to the Chrome Remote Desktop peer (either a "
        "Chrome Remote Desktop host or client) via the Chrome Remote Desktop "
        "backend."
      trigger:
        "Initiating a Chrome Remote Desktop connection."
      user_data {
        type: CREDENTIALS
      }
      data:
        "User's auth code and Chrome Remote Desktop P2P signaling messages. "
        "This includes session authentication data, SDP (Session Description "
        "Protocol) messages, and ICE (Interactive Connectivity Establishment) "
        "candidates. Details can be found at "
        "https://tools.ietf.org/html/rfc4566 and "
        "https://tools.ietf.org/html/rfc5245."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts { email: "garykac@chromium.org" }
        contacts { email: "jamiewalch@chromium.org" }
        contacts { email: "joedow@chromium.org" }
        contacts { email: "lambroslambrou@chromium.org" }
        contacts { email: "rkjnsn@chromium.org" }
        contacts { email: "yuweih@chromium.org" }
      }
      last_reviewed: "2023-07-07"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request cannot be stopped in settings, but will not be sent "
        "if the user does not use Chrome Remote Desktop."
      chrome_policy {
        RemoteAccessHostAllowRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowRemoteSupportConnections: false
        }
        RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
          policy_options {mode: MANDATORY}
          RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
        }
      }
    })");

constexpr base::TimeDelta kInboxMessageTtl = base::Minutes(1);

}  // namespace

FtlMessagingClient::FtlMessagingClient(
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    RegistrationManager* registration_manager,
    SignalingTracker* signaling_tracker)
    : FtlMessagingClient(
          std::make_unique<ProtobufHttpClient>(
              FtlServicesContext::GetServerEndpoint(),
              token_getter,
              url_loader_factory),
          registration_manager,
          std::make_unique<FtlMessageReceptionChannel>(signaling_tracker)) {}

FtlMessagingClient::FtlMessagingClient(
    std::unique_ptr<ProtobufHttpClient> client,
    RegistrationManager* registration_manager,
    std::unique_ptr<MessageReceptionChannel> channel) {
  DCHECK(client);
  DCHECK(registration_manager);
  DCHECK(channel);
  client_ = std::move(client);
  registration_manager_ = registration_manager;
  reception_channel_ = std::move(channel);
  reception_channel_->Initialize(
      base::BindRepeating(&FtlMessagingClient::OpenReceiveMessagesStream,
                          base::Unretained(this)),
      base::BindRepeating(&FtlMessagingClient::OnMessageReceived,
                          base::Unretained(this)));
}

FtlMessagingClient::~FtlMessagingClient() = default;

base::CallbackListSubscription FtlMessagingClient::RegisterMessageCallback(
    const MessageCallback& callback) {
  return callback_list_.Add(callback);
}

void FtlMessagingClient::SendMessage(
    const std::string& destination,
    const std::string& destination_registration_id,
    const ftl::ChromotingMessage& message,
    DoneCallback on_done) {
  auto request = std::make_unique<ftl::InboxSendRequest>();
  *request->mutable_header() = FtlServicesContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  request->set_time_to_live(kInboxMessageTtl.InMicroseconds());
  *request->mutable_dest_id() =
      FtlServicesContext::CreateIdFromString(destination);

  std::string serialized_message;
  bool succeeded = message.SerializeToString(&serialized_message);
  DCHECK(succeeded);

  request->mutable_message()->set_message(serialized_message);
  request->mutable_message()->set_message_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  request->mutable_message()->set_message_type(
      ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  request->mutable_message()->set_message_class(
      ftl::InboxMessage_MessageClass_STATUS);
  if (!destination_registration_id.empty()) {
    request->add_dest_registration_ids(destination_registration_id);
  }

  ExecuteRequest(kSendMessageTrafficAnnotation, kSendMessagePath,
                 std::move(request), &FtlMessagingClient::OnSendMessageResponse,
                 std::move(on_done));
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

template <typename CallbackFunctor>
void FtlMessagingClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& tag,
    const std::string& path,
    std::unique_ptr<google::protobuf::MessageLite> request,
    CallbackFunctor callback_functor,
    DoneCallback on_done) {
  auto config = std::make_unique<ProtobufHttpRequestConfig>(tag);
  config->request_message = std::move(request);
  config->path = path;
  auto http_request = std::make_unique<ProtobufHttpRequest>(std::move(config));
  http_request->SetResponseCallback(base::BindOnce(
      callback_functor, base::Unretained(this), std::move(on_done)));
  client_->ExecuteRequest(std::move(http_request));
}

void FtlMessagingClient::OnSendMessageResponse(
    DoneCallback on_done,
    const ProtobufHttpStatus& status,
    std::unique_ptr<ftl::InboxSendResponse> response) {
  std::move(on_done).Run(status);
}

void FtlMessagingClient::BatchAckMessages(
    const ftl::BatchAckMessagesRequest& request,
    DoneCallback on_done) {
  // BatchAckMessages has a limit of 10 acks per call. Currently we ack one
  // message at a time, but check here to be safe.
  DCHECK_LE(request.message_ids_size(), 10);
  VLOG(1) << "Acking " << request.message_ids_size() << " messages";

  ExecuteRequest(kAckMessagesTrafficAnnotation, kBatchAckMessagesPath,
                 std::make_unique<ftl::BatchAckMessagesRequest>(request),
                 &FtlMessagingClient::OnBatchAckMessagesResponse,
                 std::move(on_done));
}

void FtlMessagingClient::OnBatchAckMessagesResponse(
    DoneCallback on_done,
    const ProtobufHttpStatus& status,
    std::unique_ptr<ftl::BatchAckMessagesResponse> response) {
  // TODO(yuweih): Handle failure.
  std::move(on_done).Run(status);
}

std::unique_ptr<ScopedProtobufHttpRequest>
FtlMessagingClient::OpenReceiveMessagesStream(
    base::OnceClosure on_channel_ready,
    const base::RepeatingCallback<
        void(std::unique_ptr<ftl::ReceiveMessagesResponse>)>& on_incoming_msg,
    base::OnceCallback<void(const ProtobufHttpStatus&)> on_channel_closed) {
  auto request = std::make_unique<ftl::ReceiveMessagesRequest>();
  *request->mutable_header() = FtlServicesContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());

  auto config = std::make_unique<ProtobufHttpRequestConfig>(
      kReceiveMessagesTrafficAnnotation);
  config->request_message = std::move(request);
  config->path = kReceiveMessagesPath;
  auto stream_request =
      std::make_unique<ProtobufHttpStreamRequest>(std::move(config));
  stream_request->SetStreamReadyCallback(std::move(on_channel_ready));
  stream_request->SetMessageCallback(on_incoming_msg);
  stream_request->SetStreamClosedCallback(std::move(on_channel_closed));
  auto request_holder = stream_request->CreateScopedRequest();
  client_->ExecuteRequest(std::move(stream_request));

  return request_holder;
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
  ftl::BatchAckMessagesRequest request;
  *request.mutable_header() = FtlServicesContext::CreateRequestHeader(
      registration_manager_->GetFtlAuthToken());
  request.add_message_ids(message.message_id());
  BatchAckMessages(request, base::DoNothing());
}

}  // namespace remoting
