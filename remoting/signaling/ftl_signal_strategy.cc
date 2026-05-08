// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_signal_strategy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
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
#include "remoting/signaling/jingle_message_proto_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
GetNotFoundRetryPolicy() {
  auto simple_policy = ProtobufHttpRequestConfig::GetSimpleRetryPolicy();
  auto policy = base::MakeRefCounted<ProtobufHttpRequestConfig::RetryPolicy>();
  policy->backoff_policy = simple_policy->backoff_policy;
  policy->retry_timeout = simple_policy->retry_timeout;
  policy->retriable_error_codes = {HttpStatus::Code::NOT_FOUND};
  return policy;
}

}  // namespace

class FtlSignalStrategy::Core {
 public:
  Core(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
       std::unique_ptr<RegistrationManager> registration_manager,
       std::unique_ptr<FtlMessagingClient> messaging_client);

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
  bool SendMessage(JingleMessage&& message);
  bool SendReply(JingleMessageReply&& message);
  void AddFtlListener(FtlListener* listener);
  void RemoveFtlListener(FtlListener* listener);
  bool SendFtlMessage(const SignalingAddress& destination_address,
                      ftl::ChromotingMessage&& message);
  void OnMessageReceived(const SignalingAddress& sender_address,
                         const ftl::ChromotingMessage& message);
  bool IsSignInError() const;

 private:
  template <typename T>
  bool Send(T&& message,
            const char* message_type,
            scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
                retry_policy = nullptr);
  // Methods are called in the order below when Connect() is called.
  void OnGetOAuthTokenResponse(OAuthTokenGetter::Status status,
                               const OAuthTokenInfo& token_info);
  void OnSignInGaiaResponse(const HttpStatus& status);
  void StartReceivingMessages();
  void OnReceiveMessagesStreamStarted();
  void OnReceiveMessagesStreamClosed(const HttpStatus& status);

  void SendMessageImpl(
      const SignalingAddress& receiver,
      ftl::ChromotingMessage&& message,
      FtlMessagingClient::DoneCallback callback,
      scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> retry_policy =
          nullptr);
  void OnSendMessageResponse(const SignalingAddress& receiver,
                             const std::string& stanza_id,
                             const HttpStatus& status);

  // Returns true if the status is handled.
  void HandleHttpStatusError(const base::Location& location,
                             const HttpStatus& status);

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;

  std::unique_ptr<RegistrationManager> registration_manager_;
  std::unique_ptr<FtlMessagingClient> messaging_client_;

  std::string user_email_;
  SignalingAddress local_address_;

  base::CallbackListSubscription receive_message_subscription_;

  Error error_ = OK;
  bool is_sign_in_error_ = false;

  base::ObserverList<Listener, true> listeners_;
  base::ObserverList<FtlListener, true> ftl_listeners_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_{this};
};

FtlSignalStrategy::Core::Core(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<RegistrationManager> registration_manager,
    std::unique_ptr<FtlMessagingClient> messaging_client) {
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
    observer.OnSignalingStateChanged(CONNECTING);
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
      observer.OnSignalingStateChanged(DISCONNECTED);
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

void FtlSignalStrategy::Core::AddFtlListener(FtlListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_listeners_.AddObserver(listener);
}

void FtlSignalStrategy::Core::RemoveFtlListener(FtlListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_listeners_.RemoveObserver(listener);
}

bool FtlSignalStrategy::Core::SendMessage(JingleMessage&& message) {
  // Note that duplicate messages may be sent, but the client and host are
  // responsible for filtering out duplicates.
  scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> policy;
  if (message.action() == JingleMessage::ActionType::kSessionAccept) {
    policy = GetNotFoundRetryPolicy();
  }

  return Send(std::move(message), "message", std::move(policy));
}

bool FtlSignalStrategy::Core::SendReply(JingleMessageReply&& message) {
  // Generally we don't want to retry replies either, but session-initiate
  // replies have been observed to be rejected with NOT_FOUND, possible due to
  // replicate delays in the back-end. Since we don't know here what message
  // we are replying to, we consider NOT_FOUND to be retriable here.
  return Send(std::move(message), "reply", GetNotFoundRetryPolicy());
}

template <typename T>
bool FtlSignalStrategy::Core::Send(
    T&& message,
    const char* message_type,
    scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> retry_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != CONNECTED) {
    HOST_LOG << "Dropping " << message_type << " because FTL is not connected.";
    return false;
  }

  // Synthesizing the from attribute in the message.
  message.from = local_address_;

  std::string message_id = message.message_id;
  SignalingAddress destination_address = message.to;
  ftl::ChromotingMessage crd_message;
  auto* xmpp = crd_message.mutable_xmpp();
  // TODO: joedow - Stop populating the `stanza` proto field once all clients
  // in the field have been updated to handle `iq_stanza`.
  xmpp->set_stanza(message.ToSerializedXml());
  // TODO: crbug.com/504910955 - Re-enable iq_stanza once parsing issues are
  // resolved.

  auto done_callback =
      base::BindOnce(&Core::OnSendMessageResponse, weak_factory_.GetWeakPtr(),
                     destination_address, message_id);

  SendMessageImpl(destination_address, std::move(crd_message),
                  std::move(done_callback), std::move(retry_policy));
  return GetState() == CONNECTED;
}

bool FtlSignalStrategy::Core::SendFtlMessage(
    const SignalingAddress& destination_address,
    ftl::ChromotingMessage&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != CONNECTED) {
    HOST_LOG << "Dropping message because FTL is not connected.";
    return false;
  }

  SendMessageImpl(
      destination_address, std::move(message),
      base::BindOnce(&Core::OnSendMessageResponse, weak_factory_.GetWeakPtr(),
                     destination_address, std::string()),
      /*retry_policy=*/nullptr);

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
    observer.OnSignalingStateChanged(CONNECTED);
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
    const ftl::ChromotingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sender_address.channel() != SignalingAddress::Channel::FTL) {
    LOG(WARNING) << "Ignoring message sent from non-FTL JID.";
    return;
  }

  for (auto& listener : ftl_listeners_) {
    if (listener.OnIncomingFtlMessage(sender_address, message)) {
      return;
    }
  }

  if (!message.has_xmpp()) {
    return;
  }

  std::optional<SignalStrategy::Message> parsed_message;
  // We prefer the structured iq_stanza if it is present.
  // TODO: crbug.com/504910955 - Re-enable iq_stanza parsing once the issues
  // with missing fields are resolved.

  if (!parsed_message && message.xmpp().has_stanza()) {
    parsed_message = SignalStrategy::ParseStanzaXml(message.xmpp().stanza());
  }

  if (!parsed_message) {
    return;
  }

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

  for (auto& listener : listeners_) {
    if (const auto* jm = std::get_if<JingleMessage>(&*parsed_message)) {
      if (listener.OnSignalingMessage(sender_address, *jm)) {
        return;
      }
    } else {
      if (listener.OnSignalingReply(
              sender_address, std::get<JingleMessageReply>(*parsed_message))) {
        return;
      }
    }
  }
}

void FtlSignalStrategy::Core::SendMessageImpl(
    const SignalingAddress& receiver,
    ftl::ChromotingMessage&& message,
    FtlMessagingClient::DoneCallback callback,
    scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy> retry_policy) {
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
  if (message.has_xmpp()) {
    if (message.xmpp().has_iq_stanza()) {
      message_payload = message.xmpp().iq_stanza().DebugString();
    } else {
      message_payload = message.xmpp().stanza();
    }
  } else if (message.has_echo()) {
    message_payload = message.echo().message();
  } else {
    message_payload = "Error displaying message due to unknown format.";
  }

  HOST_LOG << "Sending outgoing message:\n"
           << "Receiver: " << receiver_username << "\n"
           << "Receiver registration ID: " << receiver_registration_id << "\n"
           << message_payload
           << "\n=========================================================";

  messaging_client_->SendMessage(receiver, std::move(message),
                                 std::move(callback), std::move(retry_policy));
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
  JingleMessageReply error_reply(JingleMessageReply::ErrorType::UNSPECIFIED);
  error_reply.to = local_address_;
  error_reply.from = receiver;
  error_reply.message_id = stanza_id;

  ftl::ChromotingMessage crd_message;
  auto* xmpp = crd_message.mutable_xmpp();
  xmpp->set_stanza(error_reply.ToSerializedXml());
  // TODO: crbug.com/504910955 - Re-enable iq_stanza once issues are fixed.
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
    std::unique_ptr<FtlMessagingClient> messaging_client) {
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

void FtlSignalStrategy::AddFtlListener(FtlListener* listener) {
  core_->AddFtlListener(listener);
}

void FtlSignalStrategy::RemoveFtlListener(FtlListener* listener) {
  core_->RemoveFtlListener(listener);
}

bool FtlSignalStrategy::SendMessage(JingleMessage&& message) {
  return core_->SendMessage(std::move(message));
}

bool FtlSignalStrategy::SendReply(JingleMessageReply&& message) {
  return core_->SendReply(std::move(message));
}

bool FtlSignalStrategy::SendFtlMessage(
    const SignalingAddress& destination_address,
    ftl::ChromotingMessage&& message) {
  return core_->SendFtlMessage(destination_address, std::move(message));
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
    std::unique_ptr<FtlMessagingClient> messaging_client) {
  core_ = std::make_unique<Core>(std::move(oauth_token_getter),
                                 std::move(registration_manager),
                                 std::move(messaging_client));
}

FtlSignalStrategy::FtlSignalStrategy() = default;

}  // namespace remoting
