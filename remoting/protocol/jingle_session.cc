// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/session_observer.h"
#include "remoting/protocol/session_plugin.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/webrtc/api/candidate.h"

using jingle_xmpp::XmlElement;

namespace remoting::protocol {

namespace {

// Timeouts have been temporarily increased for testing.
// TODO(rkjnsn): Revert default and session timeouts once done with testing.

// How long we should wait for a response from the other end. This value is used
// for all requests except |transport-info|.
// const int kDefaultMessageTimeout = 10;
const int kDefaultMessageTimeout = 35;  // For testing

// During a reconnection, it usually takes longer for the peer to respond due to
// pending messages in the channel from the previous session.  From experiment,
// it can take up to 20s for the session to reconnect. To make it safe, setting
// the timeout to 30s.
// const int kSessionInitiateAndAcceptTimeout = kDefaultMessageTimeout * 3;
const int kSessionInitiateAndAcceptTimeout = 45;  // For testing

// Timeout for the transport-info messages.
const int kTransportInfoTimeout = 10 * 60;

// Special value for an invalid sequential ID for an incoming IQ.
const int kInvalid = -1;

// Special value indicating that any sequential ID is valid for the next
// incoming IQ.
const int kAny = -1;

ErrorCode AuthRejectionReasonToErrorCode(
    Authenticator::RejectionReason reason) {
  switch (reason) {
    case Authenticator::RejectionReason::INVALID_CREDENTIALS:
      return ErrorCode::AUTHENTICATION_FAILED;
    case Authenticator::RejectionReason::PROTOCOL_ERROR:
      return ErrorCode::INCOMPATIBLE_PROTOCOL;
    case Authenticator::RejectionReason::INVALID_ACCOUNT_ID:
      return ErrorCode::INVALID_ACCOUNT;
    case Authenticator::RejectionReason::TOO_MANY_CONNECTIONS:
      return ErrorCode::SESSION_REJECTED;
    case Authenticator::RejectionReason::REJECTED_BY_USER:
      return ErrorCode::SESSION_REJECTED;
    case Authenticator::RejectionReason::AUTHZ_POLICY_CHECK_FAILED:
      return ErrorCode::AUTHZ_POLICY_CHECK_FAILED;
    case Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED:
      return ErrorCode::REAUTHZ_POLICY_CHECK_FAILED;
    case Authenticator::RejectionReason::LOCATION_AUTHZ_POLICY_CHECK_FAILED:
      return ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED;
    case Authenticator::RejectionReason::UNAUTHORIZED_ACCOUNT:
      return ErrorCode::UNAUTHORIZED_ACCOUNT;
    case Authenticator::RejectionReason::NO_COMMON_AUTH_METHOD:
      return ErrorCode::NO_COMMON_AUTH_METHOD;
  }
}

// Extracts a sequential id from the id attribute of the IQ stanza.
int GetSequentialId(const std::string& id) {
  std::vector<std::string> tokens =
      SplitString(id, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // Legacy endpoints does not encode the IQ ordering in the ID attribute
  if (tokens.size() != 2) {
    return kInvalid;
  }

  int result = kInvalid;
  if (!base::StringToInt(tokens[1], &result)) {
    return kInvalid;
  }
  return result;
}

}  // namespace

// A Queue that sorts incoming messages and returns them in the ascending order
// of sequence ids. The sequence id can be extracted from the ID attribute of
// an IQ stanza, which have the following format <opaque_string>_<sequence_id>.
//
// Background:
// The chromoting signaling channel does not guarantee that the incoming IQs are
// delivered in the order that it is sent.
//
// This behavior leads to transient session setup failures.  For instance,
// a <transport-info> that is sent after a <session-info> message is sometimes
// delivered to the client out of order, causing the client to close the
// session due to an unexpected request.
class JingleSession::OrderedMessageQueue {
 public:
  OrderedMessageQueue() = default;

  OrderedMessageQueue(const OrderedMessageQueue&) = delete;
  OrderedMessageQueue& operator=(const OrderedMessageQueue&) = delete;

  ~OrderedMessageQueue() = default;

  // Returns the list of messages ordered by their sequential IDs.
  std::vector<PendingMessage> OnIncomingMessage(
      const std::string& id,
      PendingMessage&& pending_message);

  // Sets the initial ID of the session initiate message.
  void SetInitialId(const std::string& id);

 private:
  // Implements an ordered list by using map with the |sequence_id| as the key,
  // so that |queue_| is always sorted by |sequence_id|.
  std::map<int, PendingMessage> queue_;

  int next_incoming_ = kAny;
};

std::vector<JingleSession::PendingMessage>
JingleSession::OrderedMessageQueue::OnIncomingMessage(
    const std::string& id,
    JingleSession::PendingMessage&& message) {
  std::vector<JingleSession::PendingMessage> result;
  int current = GetSequentialId(id);
  // If there is no sequencing order encoded in the id, just return the
  // message.
  if (current == kInvalid) {
    result.push_back(std::move(message));
    return result;
  }

  if (next_incoming_ == kAny) {
    next_incoming_ = current;
  }

  // Ensure there are no duplicate sequence ids.
  DCHECK_GE(current, next_incoming_);
  DCHECK(queue_.find(current) == queue_.end());

  queue_.insert(std::make_pair(current, std::move(message)));

  auto it = queue_.begin();
  while (it != queue_.end() && it->first == next_incoming_) {
    result.push_back(std::move(it->second));
    it = queue_.erase(it);
    next_incoming_++;
  }

  if (current - next_incoming_ >= 3) {
    LOG(WARNING) << "Multiple messages are missing: expected= "
                 << next_incoming_ << " current= " << current;
  }
  return result;
}

void JingleSession::OrderedMessageQueue::SetInitialId(const std::string& id) {
  int current = GetSequentialId(id);
  if (current != kInvalid) {
    next_incoming_ = current + 1;
  }
}

JingleSession::PendingMessage::PendingMessage() = default;
JingleSession::PendingMessage::PendingMessage(PendingMessage&& moved) = default;
JingleSession::PendingMessage::PendingMessage(
    std::unique_ptr<JingleMessage> message,
    ReplyCallback reply_callback)
    : message(std::move(message)), reply_callback(std::move(reply_callback)) {}
JingleSession::PendingMessage::~PendingMessage() = default;

JingleSession::PendingMessage& JingleSession::PendingMessage::operator=(
    PendingMessage&& moved) = default;

JingleSession::JingleSession(JingleSessionManager* session_manager)
    : session_manager_(session_manager),
      event_handler_(nullptr),
      state_(INITIALIZING),
      error_(ErrorCode::OK),
      message_queue_(new OrderedMessageQueue) {}

JingleSession::~JingleSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  session_manager_->SessionDestroyed(this);
}

void JingleSession::SetEventHandler(Session::EventHandler* event_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(event_handler);
  event_handler_ = event_handler;
}

ErrorCode JingleSession::error() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return error_;
}

void JingleSession::StartConnection(
    const SignalingAddress& peer_address,
    std::unique_ptr<Authenticator> authenticator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::MESSAGE_READY);

  peer_address_ = peer_address;
  authenticator_ = std::move(authenticator);
  authenticator_->set_state_change_after_accepted_callback(base::BindRepeating(
      &JingleSession::OnAuthenticatorStateChangeAfterAccepted,
      base::Unretained(this)));

  // Generate random session ID. There are usually not more than 1
  // concurrent session per host, so a random 64-bit integer provides
  // enough entropy. In the worst case connection will fail when two
  // clients generate the same session ID concurrently.
  session_id_ = base::NumberToString(
      base::RandGenerator(std::numeric_limits<uint64_t>::max()));

  // Delay sending session-initiate message to ensure SessionPlugin can be
  // attached before the message.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&JingleSession::SendSessionInitiateMessage,
                                weak_factory_.GetWeakPtr()));

  SetState(CONNECTING);
}

void JingleSession::InitializeIncomingConnection(
    const std::string& message_id,
    const JingleMessage& initiate_message,
    std::unique_ptr<Authenticator> authenticator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initiate_message.description.get());
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::WAITING_MESSAGE);

  peer_address_ = initiate_message.from;
  authenticator_ = std::move(authenticator);
  authenticator_->set_state_change_after_accepted_callback(base::BindRepeating(
      &JingleSession::OnAuthenticatorStateChangeAfterAccepted,
      base::Unretained(this)));
  session_id_ = initiate_message.sid;
  message_queue_->SetInitialId(message_id);

  SetState(ACCEPTING);

  config_ =
      SessionConfig::SelectCommon(initiate_message.description->config(),
                                  session_manager_->protocol_config_.get());
  if (!config_) {
    LOG(WARNING) << "Rejecting connection from " << peer_address_.id()
                 << " because no compatible configuration has been found.";
    Close(ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }
}

void JingleSession::AcceptIncomingConnection(
    const JingleMessage& initiate_message) {
  DCHECK(config_);

  ProcessIncomingPluginMessage(initiate_message);
  // Process the first authentication message.
  const jingle_xmpp::XmlElement* first_auth_message =
      initiate_message.description->authenticator_message();

  if (!first_auth_message) {
    Close(ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
  // |authenticator_| is owned, so Unretained() is safe here.
  authenticator_->ProcessMessage(
      first_auth_message,
      base::BindOnce(&JingleSession::ContinueAcceptIncomingConnection,
                     base::Unretained(this)));
}

void JingleSession::ContinueAcceptIncomingConnection() {
  DCHECK_NE(authenticator_->state(), Authenticator::PROCESSING_MESSAGE);
  if (authenticator_->state() == Authenticator::REJECTED) {
    Close(AuthRejectionReasonToErrorCode(authenticator_->rejection_reason()));
    return;
  }

  // Send the session-accept message.
  std::unique_ptr<JingleMessage> message(new JingleMessage(
      peer_address_, JingleMessage::SESSION_ACCEPT, session_id_));

  std::unique_ptr<jingle_xmpp::XmlElement> auth_message;
  if (authenticator_->state() == Authenticator::MESSAGE_READY) {
    auth_message = authenticator_->GetNextMessage();
  }

  message->description = std::make_unique<ContentDescription>(
      CandidateSessionConfig::CreateFrom(*config_), std::move(auth_message));
  SendMessage(std::move(message));

  // Update state.
  SetState(ACCEPTED);

  if (authenticator_->state() == Authenticator::ACCEPTED) {
    OnAuthenticated();
  } else {
    DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
    if (authenticator_->started()) {
      SetState(AUTHENTICATING);
    }
  }
}

const std::string& JingleSession::jid() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return peer_address_.id();
}

const SessionConfig& JingleSession::config() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return *config_;
}

const Authenticator& JingleSession::authenticator() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return *authenticator_;
}

void JingleSession::SetTransport(Transport* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!transport_);
  DCHECK(transport);
  transport_ = transport;
}

void JingleSession::SendTransportInfo(
    std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, AUTHENTICATED);

  std::unique_ptr<JingleMessage> message(new JingleMessage(
      peer_address_, JingleMessage::TRANSPORT_INFO, session_id_));
  message->transport_info = std::move(transport_info);
  AddPluginAttachments(message.get());

  std::unique_ptr<jingle_xmpp::XmlElement> stanza = message->ToXml();
  stanza->AddAttr(kQNameId, GetNextOutgoingId());

  auto request = session_manager_->iq_sender()->SendIq(
      std::move(stanza), base::BindOnce(&JingleSession::OnTransportInfoResponse,
                                        base::Unretained(this)));
  if (request) {
    request->SetTimeout(base::Seconds(kTransportInfoTimeout));
    transport_info_requests_.push_back(std::move(request));
  } else {
    LOG(ERROR) << "Failed to send a transport-info message";
  }
}

void JingleSession::Close(protocol::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_session_active()) {
    // Send session-terminate message with the appropriate error code.
    JingleMessage::Reason reason;
    switch (error) {
      case ErrorCode::OK:
        reason = JingleMessage::SUCCESS;
        break;
      case ErrorCode::SESSION_REJECTED:
      case ErrorCode::AUTHENTICATION_FAILED:
      case ErrorCode::INVALID_ACCOUNT:
        reason = JingleMessage::DECLINE;
        break;
      case ErrorCode::INCOMPATIBLE_PROTOCOL:
        reason = JingleMessage::INCOMPATIBLE_PARAMETERS;
        break;
      case ErrorCode::HOST_OVERLOAD:
        reason = JingleMessage::CANCEL;
        break;
      case ErrorCode::MAX_SESSION_LENGTH:
        reason = JingleMessage::EXPIRED;
        break;
      case ErrorCode::HOST_CONFIGURATION_ERROR:
        reason = JingleMessage::FAILED_APPLICATION;
        break;
      default:
        reason = JingleMessage::GENERAL_ERROR;
    }

    std::unique_ptr<JingleMessage> message(new JingleMessage(
        peer_address_, JingleMessage::SESSION_TERMINATE, session_id_));
    message->reason = reason;
    message->error_code = error;
    SendMessage(std::move(message));
  }

  error_ = error;

  if (state_ != FAILED && state_ != CLOSED) {
    if (error != ErrorCode::OK) {
      SetState(FAILED);
    } else {
      SetState(CLOSED);
    }
  }
}

void JingleSession::AddPlugin(SessionPlugin* plugin) {
  DCHECK(plugin);
  plugins_.push_back(plugin);
}

void JingleSession::SendMessage(std::unique_ptr<JingleMessage> message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (message->action != JingleMessage::SESSION_TERMINATE) {
    // When the host accepts session-initiate message from a client JID it
    // doesn't recognize it sends session-terminate without session-accept.
    // Attaching plugin information to this session-terminate message may lead
    // to privacy issues (e.g. leaking Windows version to someone who does not
    // own the host). So a simply approach is to ignore plugins when sending
    // SESSION_TERMINATE message.
    AddPluginAttachments(message.get());
  }
  std::unique_ptr<jingle_xmpp::XmlElement> stanza = message->ToXml();
  stanza->AddAttr(kQNameId, GetNextOutgoingId());

  auto request = session_manager_->iq_sender()->SendIq(
      std::move(stanza),
      base::BindOnce(&JingleSession::OnMessageResponse, base::Unretained(this),
                     message->action));

  int timeout = kDefaultMessageTimeout;
  if (message->action == JingleMessage::SESSION_INITIATE ||
      message->action == JingleMessage::SESSION_ACCEPT) {
    timeout = kSessionInitiateAndAcceptTimeout;
  }
  if (request) {
    request->SetTimeout(base::Seconds(timeout));
    pending_requests_.push_back(std::move(request));
  } else {
    LOG(ERROR) << "Failed to send a "
               << JingleMessage::GetActionName(message->action) << " message";
  }
}

void JingleSession::OnMessageResponse(JingleMessage::ActionType request_type,
                                      IqRequest* request,
                                      const jingle_xmpp::XmlElement* response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Delete the request from the list of pending requests.
  pending_requests_.erase(base::ranges::find(pending_requests_, request,
                                             &std::unique_ptr<IqRequest>::get));

  // Ignore all responses after session was closed.
  if (state_ == CLOSED || state_ == FAILED) {
    return;
  }

  std::string type_str = JingleMessage::GetActionName(request_type);

  // |response| will be nullptr if the request timed out.
  if (!response) {
    LOG(ERROR) << type_str << " request timed out.";
    Close(ErrorCode::SIGNALING_TIMEOUT);
    return;
  } else {
    const std::string& type =
        response->Attr(jingle_xmpp::QName(std::string(), "type"));
    if (type != "result") {
      LOG(ERROR) << "Received error in response to " << type_str
                 << " message: \"" << response->Str()
                 << "\". Terminating the session.";

      // TODO(sergeyu): There may be different reasons for error
      // here. Parse the response stanza to find failure reason.
      Close(ErrorCode::PEER_IS_OFFLINE);
    }
  }
}

void JingleSession::OnTransportInfoResponse(
    IqRequest* request,
    const jingle_xmpp::XmlElement* response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!transport_info_requests_.empty());

  // Consider transport-info requests sent before this one lost and delete
  // all IqRequest objects in front of |request|.
  auto request_it = base::ranges::find(transport_info_requests_, request,
                                       &std::unique_ptr<IqRequest>::get);
  DCHECK(request_it != transport_info_requests_.end());
  transport_info_requests_.erase(transport_info_requests_.begin(),
                                 request_it + 1);

  // Ignore transport-info timeouts.
  if (!response) {
    LOG(ERROR) << "transport-info request has timed out.";
    return;
  }

  const std::string& type =
      response->Attr(jingle_xmpp::QName(std::string(), "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to transport-info message: \""
               << response->Str() << "\". Terminating the session.";
    Close(ErrorCode::PEER_IS_OFFLINE);
  }
}

void JingleSession::OnIncomingMessage(const std::string& id,
                                      std::unique_ptr<JingleMessage> message,
                                      ReplyCallback reply_callback) {
  ProcessIncomingPluginMessage(*message);
  std::vector<PendingMessage> ordered = message_queue_->OnIncomingMessage(
      id, PendingMessage{std::move(message), std::move(reply_callback)});
  base::WeakPtr<JingleSession> self = weak_factory_.GetWeakPtr();
  for (auto& pending_message : ordered) {
    ProcessIncomingMessage(std::move(pending_message.message),
                           std::move(pending_message.reply_callback));
    if (!self) {
      return;
    }
  }
}

void JingleSession::ProcessIncomingMessage(
    std::unique_ptr<JingleMessage> message,
    ReplyCallback reply_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (peer_address_ != message->from) {
    // Ignore messages received from a different Jid.
    std::move(reply_callback).Run(JingleMessageReply::INVALID_SID);
    return;
  }

  switch (message->action) {
    case JingleMessage::SESSION_ACCEPT:
      OnAccept(std::move(message), std::move(reply_callback));
      break;

    case JingleMessage::SESSION_INFO:
      OnSessionInfo(std::move(message), std::move(reply_callback));
      break;

    case JingleMessage::TRANSPORT_INFO:
      OnTransportInfo(std::move(message), std::move(reply_callback));
      break;

    case JingleMessage::SESSION_TERMINATE:
      OnTerminate(std::move(message), std::move(reply_callback));
      break;

    default:
      std::move(reply_callback).Run(JingleMessageReply::UNEXPECTED_REQUEST);
  }
}

void JingleSession::OnAccept(std::unique_ptr<JingleMessage> message,
                             ReplyCallback reply_callback) {
  if (state_ != CONNECTING) {
    std::move(reply_callback).Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  std::move(reply_callback).Run(JingleMessageReply::NONE);

  const jingle_xmpp::XmlElement* auth_message =
      message->description->authenticator_message();
  if (!auth_message) {
    DLOG(WARNING) << "Received session-accept without authentication message ";
    Close(ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  if (!InitializeConfigFromDescription(message->description.get())) {
    Close(ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  SetState(ACCEPTED);

  DCHECK(authenticator_->state() == Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(
      auth_message, base::BindOnce(&JingleSession::ProcessAuthenticationStep,
                                   base::Unretained(this)));
}

void JingleSession::OnSessionInfo(std::unique_ptr<JingleMessage> message,
                                  ReplyCallback reply_callback) {
  if (!message->info.get() ||
      !Authenticator::IsAuthenticatorMessage(message->info.get())) {
    std::move(reply_callback).Run(JingleMessageReply::UNSUPPORTED_INFO);
    return;
  }

  if ((state_ != ACCEPTED && state_ != AUTHENTICATING) ||
      authenticator_->state() != Authenticator::WAITING_MESSAGE) {
    LOG(WARNING) << "Received unexpected authenticator message "
                 << message->info->Str();
    std::move(reply_callback).Run(JingleMessageReply::UNEXPECTED_REQUEST);
    Close(ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  std::move(reply_callback).Run(JingleMessageReply::NONE);

  authenticator_->ProcessMessage(
      message->info.get(),
      base::BindOnce(&JingleSession::ProcessAuthenticationStep,
                     base::Unretained(this)));
}

void JingleSession::OnTransportInfo(std::unique_ptr<JingleMessage> message,
                                    ReplyCallback reply_callback) {
  if (!message->transport_info) {
    std::move(reply_callback).Run(JingleMessageReply::BAD_REQUEST);
    return;
  }

  if (state_ == AUTHENTICATING) {
    pending_transport_info_.push_back(
        PendingMessage{std::move(message), std::move(reply_callback)});
  } else if (state_ == AUTHENTICATED) {
    std::move(reply_callback)
        .Run(transport_->ProcessTransportInfo(message->transport_info.get())
                 ? JingleMessageReply::NONE
                 : JingleMessageReply::BAD_REQUEST);
  } else {
    LOG(ERROR) << "Received unexpected transport-info message.";
    std::move(reply_callback).Run(JingleMessageReply::UNEXPECTED_REQUEST);
  }
}

void JingleSession::OnTerminate(std::unique_ptr<JingleMessage> message,
                                ReplyCallback reply_callback) {
  if (!is_session_active()) {
    LOG(WARNING) << "Received unexpected session-terminate message.";
    std::move(reply_callback).Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  std::move(reply_callback).Run(JingleMessageReply::NONE);

  error_ = message->error_code;
  if (error_ == ErrorCode::UNKNOWN_ERROR) {
    // get error code from message.reason for compatibility with older versions
    // that do not add <error-code>.
    switch (message->reason) {
      case JingleMessage::SUCCESS:
        if (state_ == CONNECTING) {
          error_ = ErrorCode::SESSION_REJECTED;
        } else {
          error_ = ErrorCode::OK;
        }
        break;
      case JingleMessage::DECLINE:
        error_ = ErrorCode::AUTHENTICATION_FAILED;
        break;
      case JingleMessage::CANCEL:
        error_ = ErrorCode::HOST_OVERLOAD;
        break;
      case JingleMessage::EXPIRED:
        error_ = ErrorCode::MAX_SESSION_LENGTH;
        break;
      case JingleMessage::INCOMPATIBLE_PARAMETERS:
        error_ = ErrorCode::INCOMPATIBLE_PROTOCOL;
        break;
      case JingleMessage::FAILED_APPLICATION:
        error_ = ErrorCode::HOST_CONFIGURATION_ERROR;
        break;
      case JingleMessage::GENERAL_ERROR:
        error_ = ErrorCode::CHANNEL_CONNECTION_ERROR;
        break;
      default:
        error_ = ErrorCode::UNKNOWN_ERROR;
    }
  } else if (error_ == ErrorCode::SESSION_REJECTED) {
    // For backward compatibility, we still use AUTHENTICATION_FAILED for
    // SESSION_REJECTED error.
    // TODO(zijiehe): Handle SESSION_REJECTED error in WebApp. Tracked by
    // http://crbug.com/618036.
    error_ = ErrorCode::AUTHENTICATION_FAILED;
  }

  if (error_ != ErrorCode::OK) {
    SetState(FAILED);
  } else {
    SetState(CLOSED);
  }
}

void JingleSession::OnAuthenticatorStateChangeAfterAccepted() {
  if (authenticator_->state() == Authenticator::REJECTED) {
    Close(AuthRejectionReasonToErrorCode(authenticator_->rejection_reason()));
  } else {
    NOTREACHED() << "Unexpected authenticator state: "
                 << authenticator_->state();
  }
}

bool JingleSession::InitializeConfigFromDescription(
    const ContentDescription* description) {
  DCHECK(description);
  config_ = SessionConfig::GetFinalConfig(description->config());
  if (!config_) {
    LOG(ERROR) << "session-accept does not specify configuration";
    return false;
  }
  if (!session_manager_->protocol_config_->IsSupported(*config_)) {
    LOG(ERROR) << "session-accept specifies an invalid configuration";
    return false;
  }

  return true;
}

void JingleSession::ProcessAuthenticationStep() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(authenticator_->state(), Authenticator::PROCESSING_MESSAGE);

  if (state_ != ACCEPTED && state_ != AUTHENTICATING) {
    DCHECK(state_ == FAILED || state_ == CLOSED);
    // The remote host closed the connection while the authentication was being
    // processed asynchronously, nothing to do.
    return;
  }

  if (authenticator_->state() == Authenticator::MESSAGE_READY) {
    std::unique_ptr<JingleMessage> message(new JingleMessage(
        peer_address_, JingleMessage::SESSION_INFO, session_id_));
    message->info = authenticator_->GetNextMessage();
    DCHECK(message->info.get());
    SendMessage(std::move(message));
  }
  DCHECK_NE(authenticator_->state(), Authenticator::MESSAGE_READY);

  if (authenticator_->started()) {
    base::WeakPtr<JingleSession> self = weak_factory_.GetWeakPtr();
    SetState(AUTHENTICATING);
    if (!self) {
      return;
    }
  }

  if (authenticator_->state() == Authenticator::ACCEPTED) {
    OnAuthenticated();
  } else if (authenticator_->state() == Authenticator::REJECTED) {
    Close(AuthRejectionReasonToErrorCode(authenticator_->rejection_reason()));
  }
}

void JingleSession::OnAuthenticated() {
  transport_->Start(authenticator_.get(),
                    base::BindRepeating(&JingleSession::SendTransportInfo,
                                        weak_factory_.GetWeakPtr()));

  base::WeakPtr<JingleSession> self = weak_factory_.GetWeakPtr();
  std::vector<PendingMessage> messages_to_process;
  std::swap(messages_to_process, pending_transport_info_);
  for (auto& message : messages_to_process) {
    std::move(message.reply_callback)
        .Run(transport_->ProcessTransportInfo(
                 message.message->transport_info.get())
                 ? JingleMessageReply::NONE
                 : JingleMessageReply::BAD_REQUEST);
    if (!self) {
      return;
    }
  }

  SetState(AUTHENTICATED);
}

void JingleSession::SetState(State new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    // Observers must be called before the event handler, since the event
    // handler may destroy the session.
    for (SessionObserver& observer : session_manager_->observers_) {
      observer.OnSessionStateChange(*this, new_state);
    }
    if (event_handler_) {
      event_handler_->OnSessionStateChange(new_state);
    }
  }
}

bool JingleSession::is_session_active() {
  return state_ == CONNECTING || state_ == ACCEPTING || state_ == ACCEPTED ||
         state_ == AUTHENTICATING || state_ == AUTHENTICATED;
}

void JingleSession::ProcessIncomingPluginMessage(const JingleMessage& message) {
  if (!message.attachments) {
    return;
  }
  for (remoting::protocol::SessionPlugin* plugin : plugins_) {
    plugin->OnIncomingMessage(*(message.attachments));
  }
}

void JingleSession::AddPluginAttachments(JingleMessage* message) {
  DCHECK(message);
  for (remoting::protocol::SessionPlugin* plugin : plugins_) {
    std::unique_ptr<XmlElement> attachment = plugin->GetNextMessage();
    if (attachment) {
      message->AddAttachment(std::move(attachment));
    }
  }
}

void JingleSession::SendSessionInitiateMessage() {
  if (state_ != CONNECTING) {
    return;
  }
  std::unique_ptr<JingleMessage> message(new JingleMessage(
      peer_address_, JingleMessage::SESSION_INITIATE, session_id_));
  message->initiator =
      session_manager_->signal_strategy_->GetLocalAddress().id();
  message->description = std::make_unique<ContentDescription>(
      session_manager_->protocol_config_->Clone(),
      authenticator_->GetNextMessage());
  SendMessage(std::move(message));
}

std::string JingleSession::GetNextOutgoingId() {
  return outgoing_id_prefix_ + "_" + base::NumberToString(++next_outgoing_id_);
}

}  // namespace remoting::protocol
