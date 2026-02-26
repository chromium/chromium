// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_signal_strategy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/http_status.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/signaling/ftl_device_id_provider.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/ftl_registration_manager.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

class FtlSignalStrategy::Core {
 public:
  Core(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
       std::unique_ptr<RegistrationManager> registration_manager,
       std::unique_ptr<MessagingClient> messaging_client);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void Connect();
  void Disconnect();
  State GetState() const;
  Error GetError() const;
  const SignalingAddress& GetLocalAddress() const;
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);
  bool SendMessage(const SignalingAddress& destination_address,
                   SignalingMessage&& message);
  void OnMessageReceived(const SignalingAddress& sender_address,
                         const SignalingMessage& message);
  bool IsSignInError() const;

 private:
  // Methods are called in the order below when Connect() is called.
  void OnGetOAuthTokenResponse(OAuthTokenGetter::Status status,
                               const OAuthTokenInfo& token_info);
  void OnSignInGaiaResponse(const HttpStatus& status);
  void StartReceivingMessages();
  void OnReceiveMessagesStreamStarted();
  void OnReceiveMessagesStreamClosed(const HttpStatus& status);

  void SendMessageImpl(const SignalingAddress& receiver,
                       SignalingMessage&& message,
                       MessagingClient::DoneCallback callback);
  void OnSendMessageResponse(const SignalingAddress& receiver,
                             const std::string& stanza_id,
                             const HttpStatus& status);

  // Returns true if the status is handled.
  void HandleHttpStatusError(const base::Location& location,
                             const HttpStatus& status);

  void OnStanza(const SignalingAddress& sender_address,
                std::unique_ptr<jingle_xmpp::XmlElement> stanza);

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;

  std::unique_ptr<RegistrationManager> registration_manager_;
  std::unique_ptr<MessagingClient> messaging_client_;

  std::string user_email_;
  SignalingAddress local_address_;

  base::CallbackListSubscription receive_message_subscription_;

  Error error_ = OK;
  bool is_sign_in_error_ = false;

  base::ObserverList<Listener, true> listeners_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_{this};
};

FtlSignalStrategy::Core::Core(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<RegistrationManager> registration_manager,
    std::unique_ptr<MessagingClient> messaging_client) {
  DCHECK(oauth_token_getter);
  DCHECK(registration_manager);
  DCHECK(messaging_client);
  oauth_token_getter_ = std::move(oauth_token_getter);
  registration_manager_ = std::move(registration_manager);
  messaging_client_ = std::move(messaging_client);
}

FtlSignalStrategy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (registration_manager_) {
    registration_manager_->SignOut();
  }
  Disconnect();
}

void FtlSignalStrategy::Core::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != DISCONNECTED) {
    LOG(WARNING) << "Signaling is not disconnected. State: " << GetState();
    return;
  }

  error_ = OK;
  is_sign_in_error_ = false;

  receive_message_subscription_ =
      messaging_client_->RegisterMessageCallback(base::BindRepeating(
          &Core::OnMessageReceived, weak_factory_.GetWeakPtr()));

  for (auto& observer : listeners_) {
    observer.OnSignalStrategyStateChange(CONNECTING);
  }

  StartReceivingMessages();
}

void FtlSignalStrategy::Core::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (receive_message_subscription_) {
    local_address_ = SignalingAddress();
    receive_message_subscription_ = {};
    messaging_client_->StopReceivingMessages();

    for (auto& observer : listeners_) {
      observer.OnSignalStrategyStateChange(DISCONNECTED);
    }
  }
}

SignalStrategy::State FtlSignalStrategy::Core::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_address_.empty()) {
    DCHECK(receive_message_subscription_);
    return CONNECTED;
  } else if (receive_message_subscription_) {
    return CONNECTING;
  } else {
    return DISCONNECTED;
  }
}

SignalStrategy::Error FtlSignalStrategy::Core::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_;
}

const SignalingAddress& FtlSignalStrategy::Core::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_address_;
}

void FtlSignalStrategy::Core::AddListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void FtlSignalStrategy::Core::RemoveListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool FtlSignalStrategy::Core::SendMessage(
    const SignalingAddress& destination_address,
    SignalingMessage&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != CONNECTED) {
    HOST_LOG << "Dropping message because FTL is not connected.";
    return false;
  }

  std::unique_ptr<jingle_xmpp::XmlElement> stanza;
  if (auto* jingle_message = std::get_if<JingleMessage>(&message)) {
    // Synthesizing the from attribute in the message.
    jingle_message->from = local_address_;

    stanza = JingleMessageToXml(*jingle_message);
  } else if (auto* jingle_reply = std::get_if<JingleMessageReply>(&message)) {
    jingle_reply->from = local_address_;

    stanza = JingleMessageReplyToXml(*jingle_reply);
  }
  if (stanza) {
    std::string stanza_id = stanza->Attr(kQNameId);

    ftl::ChromotingMessage crd_message;
    crd_message.mutable_xmpp()->set_stanza(stanza->Str());
    SendMessageImpl(
        destination_address, SignalingMessage(std::move(crd_message)),
        base::BindOnce(&Core::OnSendMessageResponse, weak_factory_.GetWeakPtr(),
                       destination_address, stanza_id));
    return GetState() == CONNECTED;
  }

  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (!ftl_message) {
    LOG(ERROR) << "Tried to send a non-FTL message with FtlSignalStrategy.";
    return false;
  }

  SendMessageImpl(
      destination_address, std::move(message),
      base::BindOnce(&Core::OnSendMessageResponse, weak_factory_.GetWeakPtr(),
                     destination_address, std::string()));

  return true;
}

bool FtlSignalStrategy::Core::IsSignInError() const {
  return is_sign_in_error_;
}

void FtlSignalStrategy::Core::OnGetOAuthTokenResponse(
    OAuthTokenGetter::Status status,
    const OAuthTokenInfo& token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    switch (status) {
      case OAuthTokenGetter::Status::NETWORK_ERROR:
        error_ = SignalStrategy::Error::NETWORK_ERROR;
        break;
      case OAuthTokenGetter::Status::AUTH_ERROR:
        error_ = SignalStrategy::Error::AUTHENTICATION_FAILED;
        break;
      default:
        NOTREACHED();
    }
    is_sign_in_error_ = true;
    Disconnect();
    return;
  }

  user_email_ = token_info.user_email();
  if (user_email_.empty()) {
    LOG(WARNING) << "No user email in the OAuth token response";
    Disconnect();
    return;
  }
  StartReceivingMessages();
}

void FtlSignalStrategy::Core::OnSignInGaiaResponse(const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.ok()) {
    is_sign_in_error_ = true;
    HandleHttpStatusError(FROM_HERE, status);
    return;
  }
  StartReceivingMessages();
}

void FtlSignalStrategy::Core::StartReceivingMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(CONNECTING, GetState());
  DCHECK(!messaging_client_->IsReceivingMessages());

  if (user_email_.empty()) {
    oauth_token_getter_->CallWithToken(base::BindOnce(
        &Core::OnGetOAuthTokenResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  if (!registration_manager_->IsSignedIn()) {
    registration_manager_->SignInGaia(base::BindOnce(
        &Core::OnSignInGaiaResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  messaging_client_->StartReceivingMessages(
      base::BindOnce(&Core::OnReceiveMessagesStreamStarted,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&Core::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void FtlSignalStrategy::Core::OnReceiveMessagesStreamStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_address_ = SignalingAddress::CreateFtlSignalingAddress(
      user_email_, registration_manager_->GetRegistrationId());

  for (auto& observer : listeners_) {
    observer.OnSignalStrategyStateChange(CONNECTED);
  }
}

void FtlSignalStrategy::Core::OnReceiveMessagesStreamClosed(
    const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.error_code() == HttpStatus::Code::CANCELLED) {
    LOG(WARNING) << "ReceiveMessages stream closed with CANCELLED code.";
  }
  DCHECK(!status.ok());
  HandleHttpStatusError(FROM_HERE, status);
}

void FtlSignalStrategy::Core::OnMessageReceived(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sender_address.channel() != SignalingAddress::Channel::FTL) {
    LOG(WARNING) << "Ignoring message sent from non-FTL JID.";
    return;
  }

  SignalingMessage message_to_dispatch = message;

  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (ftl_message && ftl_message->has_xmpp() &&
      ftl_message->xmpp().has_stanza()) {
    auto parsed_message =
        SignalStrategy::ParseStanzaXml(ftl_message->xmpp().stanza());
    if (parsed_message) {
      // Validate the schema and FTL IDs.
      SignalingAddress from;
      SignalingAddress to;
      if (const auto* jm = std::get_if<JingleMessage>(&*parsed_message)) {
        from = jm->from;
        to = jm->to;
      } else if (const auto* jmr =
                     std::get_if<JingleMessageReply>(&*parsed_message)) {
        from = jmr->from;
        to = jmr->to;
      } else {
        LOG(WARNING) << "Received unexpected non-IQ packet";
        return;
      }

      if (from != sender_address) {
        LOG(WARNING) << "Expected sender: " << sender_address.id()
                     << ", but received: " << from.id();
        return;
      }
      if (to != local_address_) {
        LOG(WARNING) << "Expected receiver: " << local_address_.id()
                     << ", but received: " << to.id();
        return;
      }
      message_to_dispatch = std::move(*parsed_message);
    } else {
      // Parsing failed.
      return;
    }
  }

  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingMessage(sender_address,
                                                 message_to_dispatch)) {
      return;
    }
  }
}

void FtlSignalStrategy::Core::SendMessageImpl(
    const SignalingAddress& receiver,
    SignalingMessage&& message,
    MessagingClient::DoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string receiver_username;
  std::string receiver_registration_id;
  bool get_info_result =
      receiver.GetFtlInfo(&receiver_username, &receiver_registration_id);
  if (!get_info_result) {
    LOG(DFATAL) << "Receiver is not in FTL address: " << receiver.id();
    return;
  }

  std::string message_payload;
  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (ftl_message) {
    if (ftl_message->has_xmpp()) {
      message_payload = ftl_message->xmpp().stanza();
    } else if (ftl_message->has_echo()) {
      message_payload = ftl_message->echo().message();
    } else {
      message_payload = "Error displaying message due to unknown format.";
    }
  } else {
    message_payload = "Error displaying message due to unknown format.";
  }

  HOST_LOG << "Sending outgoing message:\n"
           << "Receiver: " << receiver_username << "\n"
           << "Receiver registration ID: " << receiver_registration_id << "\n"
           << message_payload
           << "\n=========================================================";

  messaging_client_->SendMessage(receiver, std::move(message),
                                 std::move(callback));
}

void FtlSignalStrategy::Core::OnSendMessageResponse(
    const SignalingAddress& receiver,
    const std::string& stanza_id,
    const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    return;
  }

  if (status.error_code() == HttpStatus::Code::UNAUTHENTICATED) {
    HandleHttpStatusError(FROM_HERE, status);
    return;
  }

  LOG(ERROR) << "Failed to send message to peer. Error code: "
             << static_cast<int>(status.error_code())
             << ", message: " << status.error_message();

  if (stanza_id.empty()) {
    // If the message sent was not related to signaling, then exit early.
    return;
  }

  // Fake an error message so JingleSession will take it as PEER_IS_OFFLINE.
  auto error_iq = std::make_unique<jingle_xmpp::XmlElement>(kQNameIq);
  error_iq->SetAttr(kQNameType, kIqTypeError);
  error_iq->SetAttr(kQNameId, stanza_id);
  error_iq->SetAttr(kQNameFrom, receiver.id());
  error_iq->SetAttr(kQNameTo, local_address_.id());

  ftl::ChromotingMessage crd_message;
  crd_message.mutable_xmpp()->set_stanza(error_iq->Str());
  OnMessageReceived(receiver, crd_message);
}

void FtlSignalStrategy::Core::HandleHttpStatusError(
    const base::Location& location,
    const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  // We don't map HTTP_UNAUTHORIZED to AUTHENTICATION_FAILED here, as it will
  // permanently terminate the host, which is not desirable since it might
  // happen when the FTL registration becomes invalid while the robot account
  // itself is still intact.
  // AUTHENTICATION_FAILED is only reported if the OAuthTokenGetter fails to
  // fetch the token.
  error_ = Error::NETWORK_ERROR;
  LOG(ERROR) << "Received server error. Error code: "
             << static_cast<int>(status.error_code())
             << ", message: " << status.error_message()
             << ", location: " << location.ToString();
  if (status.error_code() == HttpStatus::Code::UNAUTHENTICATED ||
      status.error_code() == HttpStatus::Code::PERMISSION_DENIED) {
    oauth_token_getter_->InvalidateCache();
    registration_manager_->SignOut();
  }
  Disconnect();
}

FtlSignalStrategy::FtlSignalStrategy(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<FtlDeviceIdProvider> device_id_provider,
    SignalingTracker* signaling_tracker) {
  // TODO(yuweih): Just make FtlMessagingClient own FtlRegistrationManager and
  // call SignInGaia() transparently.
  auto registration_manager = std::make_unique<FtlRegistrationManager>(
      oauth_token_getter.get(), url_loader_factory,
      std::move(device_id_provider));
  auto messaging_client = std::make_unique<FtlMessagingClient>(
      oauth_token_getter.get(), url_loader_factory, registration_manager.get(),
      signaling_tracker);
  CreateCore(std::move(oauth_token_getter), std::move(registration_manager),
             std::move(messaging_client));
}

FtlSignalStrategy::FtlSignalStrategy(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<RegistrationManager> registration_manager,
    std::unique_ptr<MessagingClient> messaging_client) {
  CreateCore(std::move(oauth_token_getter), std::move(registration_manager),
             std::move(messaging_client));
}

FtlSignalStrategy::~FtlSignalStrategy() {
  // All listeners should be removed at this point, so it's safe to detach
  // |core_|.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             core_.release());
}

void FtlSignalStrategy::Connect() {
  core_->Connect();
}

void FtlSignalStrategy::Disconnect() {
  core_->Disconnect();
}

SignalStrategy::State FtlSignalStrategy::GetState() const {
  return core_->GetState();
}

SignalStrategy::Error FtlSignalStrategy::GetError() const {
  return core_->GetError();
}

const SignalingAddress& FtlSignalStrategy::GetLocalAddress() const {
  return core_->GetLocalAddress();
}

void FtlSignalStrategy::AddListener(Listener* listener) {
  core_->AddListener(listener);
}

void FtlSignalStrategy::RemoveListener(Listener* listener) {
  core_->RemoveListener(listener);
}

bool FtlSignalStrategy::SendMessage(const SignalingAddress& destination_address,
                                    SignalingMessage&& message) {
  return core_->SendMessage(destination_address, std::move(message));
}

std::string FtlSignalStrategy::GetNextId() {
  return base::NumberToString(base::RandUint64());
}

bool FtlSignalStrategy::IsSignInError() const {
  return core_->IsSignInError();
}

void FtlSignalStrategy::CreateCore(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<RegistrationManager> registration_manager,
    std::unique_ptr<MessagingClient> messaging_client) {
  core_ = std::make_unique<Core>(std::move(oauth_token_getter),
                                 std::move(registration_manager),
                                 std::move(messaging_client));
}

}  // namespace remoting
