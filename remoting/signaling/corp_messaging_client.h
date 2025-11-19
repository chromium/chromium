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
#include "remoting/proto/messaging_service.h"
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

// A class for sending and receiving messages via the Corp messaging API.
class CorpMessagingClient final {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const internal::PeerMessageStruct& message)>;
  using MessageCallbackList = base::RepeatingCallbackList<void(
      const internal::PeerMessageStruct& message)>;
  using StatusCallback = base::OnceCallback<void(const HttpStatus& status)>;

  CorpMessagingClient(
      const std::string& username,
      const std::string& public_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<net::ClientCertStore> client_cert_store);

  CorpMessagingClient(const CorpMessagingClient&) = delete;
  CorpMessagingClient& operator=(const CorpMessagingClient&) = delete;

  ~CorpMessagingClient();

  base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback);

  void SendMessage(const std::string& messaging_authz_token,
                   const std::string& payload,
                   StatusCallback on_done);

  void SendTestMessage(const std::string& messaging_authz_token,
                       internal::SystemTestStruct system_test_struct,
                       StatusCallback on_done);

  void StartReceivingMessages(base::OnceClosure on_ready,
                              StatusCallback on_closed);

  void StopReceivingMessages();

  bool IsReceivingMessages() const;

 private:
  template <typename CallbackFunctor>
  void ExecuteRequest(const net::NetworkTrafficAnnotationTag& tag,
                      const std::string& path,
                      bool enable_retries,
                      std::unique_ptr<google::protobuf::MessageLite> request,
                      CallbackFunctor callback_functor,
                      StatusCallback on_done);

  void OnSendMessageResponse(
      StatusCallback on_done,
      const HttpStatus& status,
      std::unique_ptr<internal::HostSendMessageResponse> response);

  std::unique_ptr<ScopedProtobufHttpRequest> OpenReceiveMessagesStream(
      base::OnceClosure on_channel_ready,
      const CorpMessageChannelStrategy::MessageReceivedCallback& on_message,
      StatusCallback on_channel_closed);

  void OnMessageReceived(const internal::PeerMessageStruct& message);

  std::string username_;
  std::string public_key_;
  std::unique_ptr<ProtobufHttpClient> client_;
  std::unique_ptr<MessageChannel> message_channel_;
  MessageCallbackList callback_list_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_CORP_MESSAGING_CLIENT_H_
