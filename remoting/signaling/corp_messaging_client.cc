// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_messaging_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/environment_details.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_stream_request.h"
#include "remoting/base/service_urls.h"
#include "remoting/signaling/corp_message_channel_strategy.h"
#include "remoting/signaling/message_channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

constexpr net::NetworkTrafficAnnotationTag kReceiveMessagesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "corp_messaging_client_receive_client_messages",
        R"(
    semantics {
      sender: "Chrome Remote Desktop"
      description:
        "Receives signaling messages from the Chrome Remote Desktop peer "
        "(a Chrome Remote Desktop client) via a Chrome Remote Desktop server "
        "when a user initiates the connection process."
      trigger:
        "Starting the Chrome Remote Desktop agent on a Google Corp machine."
      user_data {
        type: NONE
      }
      data: "No user data is provided in the request."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts { owners: "//remoting/OWNERS" }
      }
      last_reviewed: "2025-09-05"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request cannot be stopped in settings, but will not be sent if "
        "the user does not configure a Google Corp machine for remote access."
      policy_exception_justification:
        "Not implemented."
    })");

constexpr net::NetworkTrafficAnnotationTag kSendMessageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "corp_messaging_client_send_host_message",
        R"(
    semantics {
      sender: "Chrome Remote Desktop"
      description:
        "Sends signaling messages to the Chrome Remote Desktop peer "
        "(a Chrome Remote Desktop client) via a Chrome Remote Desktop server "
        "when a user initiates the connection process."
      trigger:
        "A remote user begins the connection process to a Google Corp machine."
      user_data {
        type: NONE
      }
      data: "No user data is provided in the request."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts { owners: "//remoting/OWNERS" }
      }
      last_reviewed: "2025-09-05"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request cannot be stopped in settings, but will not be sent if "
        "the user does not configure a Google Corp machine for remote access."
      policy_exception_justification:
        "Not implemented."
    })");

}  // namespace

CorpMessagingClient::CorpMessagingClient(
    const std::string& username,
    const std::string& public_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<net::ClientCertStore> client_cert_store)
    : username_(username),
      public_key_(public_key),
      client_(std::make_unique<ProtobufHttpClient>(
          ServiceUrls::GetInstance()->remoting_corp_endpoint(),
          /*token_getter=*/nullptr,
          url_loader_factory,
          std::move(client_cert_store))) {
  auto channel_strategy = std::make_unique<CorpMessageChannelStrategy>();
  channel_strategy->Initialize(
      base::BindRepeating(&CorpMessagingClient::OpenReceiveMessagesStream,
                          base::Unretained(this)),
      base::BindRepeating(&CorpMessagingClient::OnMessageReceived,
                          base::Unretained(this)));
  message_channel_ = std::make_unique<MessageChannel>(
      std::move(channel_strategy), /*signaling_tracker=*/nullptr);
}

CorpMessagingClient::~CorpMessagingClient() = default;

base::CallbackListSubscription CorpMessagingClient::RegisterMessageCallback(
    const MessageCallback& callback) {
  return callback_list_.Add(callback);
}

void CorpMessagingClient::SendTestMessage(
    const std::string& messaging_authz_token,
    internal::SystemTestStruct system_test_struct,
    StatusCallback on_done) {
  internal::HostSendMessageRequestStruct request;
  request.messaging_authz_token = messaging_authz_token;
  request.peer_message.message_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  request.peer_message.payload = std::move(system_test_struct);

  // SendHostMessage is non-idempotent (potentially duplicate messages will be
  // sent), so retries may not be safe.
  ExecuteRequest(
      kSendMessageTrafficAnnotation,
      std::string(internal::GetHostSendMessagePath()),
      /*enable_retries=*/false, internal::GetHostSendMessageRequest(request),
      &CorpMessagingClient::OnSendMessageResponse, std::move(on_done));
}

void CorpMessagingClient::SendMessage(const std::string& messaging_authz_token,
                                      const std::string& payload,
                                      StatusCallback on_done) {
  internal::HostSendMessageRequestStruct request;
  request.messaging_authz_token = messaging_authz_token;
  request.peer_message.message_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  internal::SystemTestStruct system_test_struct;
  internal::SimpleStruct simple_struct;
  simple_struct.payload = payload;
  system_test_struct.test_message = std::move(simple_struct);
  request.peer_message.payload = std::move(system_test_struct);

  // SendHostMessage is non-idempotent (potentially duplicate messages will be
  // sent), so retries may not be safe.
  ExecuteRequest(
      kSendMessageTrafficAnnotation,
      std::string(internal::GetHostSendMessagePath()),
      /*enable_retries=*/false, internal::GetHostSendMessageRequest(request),
      &CorpMessagingClient::OnSendMessageResponse, std::move(on_done));
}

void CorpMessagingClient::StartReceivingMessages(base::OnceClosure on_ready,
                                                 StatusCallback on_closed) {
  message_channel_->StartReceivingMessages(std::move(on_ready),
                                           std::move(on_closed));
}

void CorpMessagingClient::StopReceivingMessages() {
  message_channel_->StopReceivingMessages();
}

bool CorpMessagingClient::IsReceivingMessages() const {
  return message_channel_->IsReceivingMessages();
}

template <typename CallbackFunctor>
void CorpMessagingClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& tag,
    const std::string& path,
    bool enable_retries,
    std::unique_ptr<google::protobuf::MessageLite> request,
    CallbackFunctor callback_functor,
    StatusCallback on_done) {
  auto config = std::make_unique<ProtobufHttpRequestConfig>(tag);
  config->request_message = std::move(request);
  config->path = path;
  if (enable_retries) {
    config->UseSimpleRetryPolicy();
  }
  config->authenticated = false;
  config->api_key = internal::GetRemotingCorpApiKey();
  config->provide_certificate = true;

  auto http_request = std::make_unique<ProtobufHttpRequest>(std::move(config));
  http_request->SetResponseCallback(base::BindOnce(
      callback_functor, base::Unretained(this), std::move(on_done)));
  client_->ExecuteRequest(std::move(http_request));
}

void CorpMessagingClient::OnSendMessageResponse(
    StatusCallback on_done,
    const HttpStatus& status,
    std::unique_ptr<internal::HostSendMessageResponse> response) {
  std::move(on_done).Run(status);
}

std::unique_ptr<ScopedProtobufHttpRequest>
CorpMessagingClient::OpenReceiveMessagesStream(
    base::OnceClosure on_channel_ready,
    const CorpMessageChannelStrategy::MessageReceivedCallback& on_message,
    base::OnceCallback<void(const HttpStatus&)> on_channel_closed) {
  auto config = std::make_unique<ProtobufHttpRequestConfig>(
      kReceiveMessagesTrafficAnnotation);
  internal::HostOpenChannelRequestStruct request;
  request.username = username_;
  request.host_public_key = public_key_;
  request.machine_info.version = GetBuildVersion();
  request.machine_info.operating_system_info.name = GetOperatingSystemName();
  request.machine_info.operating_system_info.version =
      GetOperatingSystemVersion();

  config->request_message = internal::GetHostOpenChannelRequest(request);
  config->path = internal::GetHostOpenChannelPath();
  config->authenticated = false;
  config->api_key = internal::GetRemotingCorpApiKey();
  config->provide_certificate = true;

  auto stream_request =
      std::make_unique<ProtobufHttpStreamRequest>(std::move(config));
  stream_request->SetStreamReadyCallback(std::move(on_channel_ready));
  stream_request->SetMessageCallback(base::BindRepeating(
      [](CorpMessageChannelStrategy::MessageReceivedCallback callback,
         std::unique_ptr<internal::HostOpenChannelResponse> response) {
        std::move(callback).Run(
            internal::GetHostOpenChannelResponseStruct(*response));
      },
      on_message));
  stream_request->SetStreamClosedCallback(std::move(on_channel_closed));
  auto request_holder = stream_request->CreateScopedRequest();
  client_->ExecuteRequest(std::move(stream_request));

  return request_holder;
}

void CorpMessagingClient::OnMessageReceived(
    const internal::PeerMessageStruct& message) {
  callback_list_.Notify(message);
}

}  // namespace remoting
