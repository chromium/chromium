// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_signal_strategy.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/http_status.h"
#include "remoting/base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

class CorpSignalStrategy::Core {
 public:
  Core(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       CreateClientCertStoreCallback client_cert_store_callback,
       const std::string& username,
       scoped_refptr<RsaKeyPair> key_pair);
  // CorpSignalStrategyTest uses a private c'tor w/ a fake messaging client.
  Core(std::unique_ptr<MessagingClient> messaging_client,
       const SignalingAddress& local_address);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void Connect();
  void Disconnect();
  SignalStrategy::State GetState() const;
  SignalStrategy::Error GetError() const;
  const SignalingAddress& GetLocalAddress() const;
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);
  bool SendMessage(const SignalingAddress& destination_address,
                   SignalingMessage&& message);
  std::string GetNextId();
  bool IsSignInError() const;

 private:
  void OnIncomingMessage(const SignalingAddress& sender_address,
                         const SignalingMessage& message);
  void OnChannelReady();
  void OnSignalingAddressChanged(const SignalingAddress& address);
  void OnChannelClosed(const HttpStatus& status);
  void SetState(State state);
  void OnStanza(const SignalingAddress& sender_address,
                std::unique_ptr<jingle_xmpp::XmlElement> stanza);

  std::unique_ptr<MessagingClient> messaging_client_;
  base::ObserverList<Listener, true> listeners_;

  State state_ = DISCONNECTED;
  Error error_ = OK;
  SignalingAddress local_address_;
  std::string messaging_authz_token_;
  int next_id_ = 0;
  bool is_signin_error_ = false;

  base::CallbackListSubscription incoming_message_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_{this};
};

CorpSignalStrategy::Core::Core(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateClientCertStoreCallback client_cert_store_callback,
    const std::string& username,
    scoped_refptr<RsaKeyPair> key_pair) {
  messaging_client_ = std::make_unique<CorpMessagingClient>(
      username, key_pair->GetPublicKey(), url_loader_factory,
      std::move(client_cert_store_callback).Run(),
      base::BindRepeating(&Core::OnSignalingAddressChanged,
                          weak_factory_.GetWeakPtr()));
}

CorpSignalStrategy::Core::Core(
    std::unique_ptr<MessagingClient> messaging_client,
    const SignalingAddress& local_address)
    : messaging_client_(std::move(messaging_client)),
      local_address_(local_address) {}

CorpSignalStrategy::Core::~Core() = default;

void CorpSignalStrategy::Core::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetState(CONNECTING);
  incoming_message_subscription_ =
      messaging_client_->RegisterMessageCallback(base::BindRepeating(
          &Core::OnIncomingMessage, weak_factory_.GetWeakPtr()));
  messaging_client_->StartReceivingMessages(
      base::BindOnce(&Core::OnChannelReady, weak_factory_.GetWeakPtr()),
      base::BindOnce(&Core::OnChannelClosed, weak_factory_.GetWeakPtr()));
}

void CorpSignalStrategy::Core::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() == DISCONNECTED) {
    return;
  }

  messaging_client_->StopReceivingMessages();
  incoming_message_subscription_ = {};
  // Don't reset `local_address_` because it is cached at a lower layer and the
  // update callback is only triggered when the cached value changes.
  messaging_authz_token_ = std::string();
  SetState(DISCONNECTED);
}

SignalStrategy::State CorpSignalStrategy::Core::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

SignalStrategy::Error CorpSignalStrategy::Core::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_;
}

const SignalingAddress& CorpSignalStrategy::Core::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_address_;
}

void CorpSignalStrategy::Core::AddListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void CorpSignalStrategy::Core::RemoveListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool CorpSignalStrategy::Core::SendMessage(
    const SignalingAddress& destination_address,
    SignalingMessage&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != CONNECTED) {
    LOG(WARNING) << "Dropping message because not connected.";
    return false;
  }

  // TODO: joedow - Use std::visit(absl::Overload(...), message->payload()).
  std::unique_ptr<jingle_xmpp::XmlElement> stanza;
  if (auto* jingle_message = std::get_if<JingleMessage>(&message)) {
    if (destination_address.empty()) {
      LOG(ERROR) << "Invalid destination address.";
      return false;
    }

    jingle_message->from = GetLocalAddress();

    stanza = JingleMessageToXml(*jingle_message);
  } else if (auto* jingle_reply = std::get_if<JingleMessageReply>(&message)) {
    if (destination_address.empty()) {
      LOG(ERROR) << "Invalid destination address.";
      return false;
    }

    jingle_reply->from = GetLocalAddress();

    stanza = JingleMessageReplyToXml(*jingle_reply);
  }
  if (stanza) {
    internal::PeerMessageStruct peer_message;
    internal::IqStanzaStruct iq_stanza;
    iq_stanza.xml = stanza->Str();
    peer_message.payload = std::move(iq_stanza);

    return SendMessage(destination_address, std::move(peer_message));
  }

  auto* peer_message = std::get_if<internal::PeerMessageStruct>(&message);
  if (!peer_message) {
    LOG(ERROR) << "Tried to send a non-corp message with CorpSignalStrategy.";
    return false;
  }
  if (messaging_authz_token_.empty()) {
    LOG(ERROR) << "Missing authz token.";
    return false;
  }

  auto on_done = base::BindOnce([](const HttpStatus& status) {
    if (!status.ok()) {
      LOG(WARNING) << "Failed to send message. Status: "
                   << static_cast<int>(status.error_code())
                   << ", message: " << status.error_message();
    }
  });
  messaging_client_->SendMessage(SignalingAddress(messaging_authz_token_),
                                 std::move(message), std::move(on_done));
  return true;
}

std::string CorpSignalStrategy::Core::GetNextId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::NumberToString(next_id_++);
}

bool CorpSignalStrategy::Core::IsSignInError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_signin_error_;
}

void CorpSignalStrategy::Core::OnIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HOST_LOG << "Received incoming message from " << sender_address.id();

  const auto* peer_message = std::get_if<internal::PeerMessageStruct>(&message);
  if (!peer_message) {
    LOG(WARNING) << "Received message with unsupported payload type.";
    return;
  }

  const auto* iq_stanza_struct =
      std::get_if<internal::IqStanzaStruct>(&peer_message->payload);
  if (!iq_stanza_struct) {
    LOG(WARNING) << "Received PeerMessageStruct with non-IqStanza payload.";
    return;
  }

  auto parsed_message = SignalStrategy::ParseStanzaXml(iq_stanza_struct->xml);
  if (!parsed_message.has_value()) {
    return;
  }

  // TODO: joedow - Associate `messaging_authz_token_` with the sender JID. One
  // way to do this is to update SignalingAddress to include a token field so
  // it is associated with the sender JID.
  const auto& authz_token = iq_stanza_struct->messaging_authz_token;
  if (authz_token.empty()) {
    LOG(WARNING) << "Received message with missing authz token.";
    return;
  }
  if (authz_token != messaging_authz_token_) {
    HOST_LOG << "Received message with new authz token: " << authz_token;
    messaging_authz_token_ = authz_token;
  }

  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingMessage(sender_address,
                                                 *parsed_message)) {
      return;
    }
  }
}

void CorpSignalStrategy::Core::OnChannelReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetState(CONNECTED);
}

void CorpSignalStrategy::Core::OnSignalingAddressChanged(
    const SignalingAddress& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Corp signaling address is: " << address.id();
  local_address_ = address;
}

void CorpSignalStrategy::Core::OnChannelClosed(const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(WARNING) << "Corp message channel closed. Status: "
               << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
  if (status.error_code() == HttpStatus::Code::UNAUTHENTICATED) {
    error_ = AUTHENTICATION_FAILED;
    is_signin_error_ = true;
  } else {
    error_ = NETWORK_ERROR;
  }
  Disconnect();
}

void CorpSignalStrategy::Core::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == state) {
    return;
  }
  state_ = state;
  for (auto& observer : listeners_) {
    observer.OnSignalStrategyStateChange(state_);
  }
}

CorpSignalStrategy::CorpSignalStrategy(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateClientCertStoreCallback client_cert_store_callback,
    const std::string& username,
    scoped_refptr<RsaKeyPair> key_pair) {
  core_ = std::make_unique<Core>(url_loader_factory,
                                 std::move(client_cert_store_callback),
                                 username, key_pair);
}

CorpSignalStrategy::CorpSignalStrategy(
    std::unique_ptr<MessagingClient> messaging_client,
    const SignalingAddress& local_address) {
  core_ = std::make_unique<Core>(std::move(messaging_client), local_address);
}

CorpSignalStrategy::~CorpSignalStrategy() {
  // All listeners should be removed so it's safe to release |core_|.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             core_.release());
}

void CorpSignalStrategy::Connect() {
  core_->Connect();
}

void CorpSignalStrategy::Disconnect() {
  core_->Disconnect();
}

SignalStrategy::State CorpSignalStrategy::GetState() const {
  return core_->GetState();
}

SignalStrategy::Error CorpSignalStrategy::GetError() const {
  return core_->GetError();
}

const SignalingAddress& CorpSignalStrategy::GetLocalAddress() const {
  return core_->GetLocalAddress();
}

void CorpSignalStrategy::AddListener(Listener* listener) {
  core_->AddListener(listener);
}

void CorpSignalStrategy::RemoveListener(Listener* listener) {
  core_->RemoveListener(listener);
}

bool CorpSignalStrategy::SendMessage(
    const SignalingAddress& destination_address,
    SignalingMessage&& message) {
  return core_->SendMessage(destination_address, std::move(message));
}

std::string CorpSignalStrategy::GetNextId() {
  return core_->GetNextId();
}

bool CorpSignalStrategy::IsSignInError() const {
  return core_->IsSignInError();
}

}  // namespace remoting
