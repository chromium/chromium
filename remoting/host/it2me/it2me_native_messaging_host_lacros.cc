// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_lacros.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/protocol/errors.h"

namespace remoting {

namespace {

constexpr int kInvalidMessageId = -1;

int GetMessageId(const base::Value::Dict& message) {
  std::optional<int> message_id = message.FindInt(kMessageId);
  return message_id.value_or(kInvalidMessageId);
}

protocol::ErrorCode SupportSessionErrorToProtocolError(
    mojom::StartSupportSessionError session_error) {
  switch (session_error) {
    case mojom::StartSupportSessionError::kExistingAdminSession:
      return protocol::ErrorCode::EXISTING_ADMIN_SESSION;
    default:
      return protocol::ErrorCode::UNKNOWN_ERROR;
  }
}

// This function checks the email address provided to see if it is properly
// formatted. It does not validate the username or domain sections.
// TODO(joedow): Move to a shared location.
bool IsValidEmailAddress(const std::string& email) {
  return base::SplitString(email, "@", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL)
             .size() == 2U;
}

// This class is JSON <-> Mojo message converter which enables communication
// between a Chrome Remote Desktop (CRD) client website instance running in
// Lacros and the CRD components running in Ash. This class should not contain
// any logic beyond message processing, validation, and conversion.
// All interactions with it must occur on the sequence it was created on.
class It2MeNativeMessagingHostLacros : public extensions::NativeMessageHost,
                                       public mojom::SupportHostObserver {
 public:
  explicit It2MeNativeMessagingHostLacros(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~It2MeNativeMessagingHostLacros() override;

  // extensions::NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  // mojom::SupportHostObserver implementation.
  void OnHostStateStarting() override;
  void OnHostStateRequestedAccessCode() override;
  void OnHostStateReceivedAccessCode(const std::string& access_code,
                                     base::TimeDelta lifetime) override;
  void OnHostStateConnecting() override;
  void OnHostStateConnected(const std::string& remote_username) override;
  void OnHostStateDisconnected(
      const std::optional<std::string>& disconnect_reason) override;
  void OnNatPolicyChanged(mojom::NatPolicyStatePtr policy_state) override;
  void OnHostStateError(int64_t error_code) override;
  void OnPolicyError() override;
  void OnInvalidDomainError() override;

  // Handlers for Mojo responses received from Ash.
  void OnSupportHostDetailsReceived(mojom::SupportHostDetailsPtr host_details);
  void OnSupportSessionStarted(mojom::StartSupportSessionResponsePtr response);

 private:
  void ProcessHello(int message_id);
  void ProcessConnect(int message_id, base::Value::Dict message);
  void ProcessDisconnect(int message_id);
  void SendMessageToClient(base::Value::Dict message) const;
  void SendErrorAndExit(const protocol::ErrorCode error_code,
                        int message_id = kInvalidMessageId) const;

  void HandleHostStateChange(It2MeHostState state,
                             base::Value::Dict message = base::Value::Dict());

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Client> client_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  int connect_response_id_ = kInvalidMessageId;
  int hello_response_id_ = kInvalidMessageId;

  bool hello_response_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  mojom::SupportHostDetailsPtr host_details_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Receiver<mojom::SupportHostObserver> support_host_observer_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::WeakPtrFactory<It2MeNativeMessagingHostLacros> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

It2MeNativeMessagingHostLacros::It2MeNativeMessagingHostLacros(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

It2MeNativeMessagingHostLacros::~It2MeNativeMessagingHostLacros() = default;

void It2MeNativeMessagingHostLacros::OnMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string type;
  base::Value::Dict contents;
  if (!ParseNativeMessageJson(message, type, contents)) {
    client_->CloseChannel(std::string());
    return;
  }

  int message_id = GetMessageId(contents);
  if (type.empty()) {
    LOG(ERROR) << "'type' not found in request.";
    SendErrorAndExit(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL, message_id);
    return;
  }

  if (type == kHelloMessage) {
    ProcessHello(message_id);
  } else if (type == kConnectMessage) {
    ProcessConnect(message_id, std::move(contents));
  } else if (type == kDisconnectMessage) {
    ProcessDisconnect(message_id);
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
    SendErrorAndExit(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL, message_id);
  }
}

void It2MeNativeMessagingHostLacros::Start(Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ = client;

  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Remoting>()) {
    LOG(ERROR) << "Remoting is not available in this version of the browser.";
    client_->CloseChannel(std::string());
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::Remoting>()->GetSupportHostDetails(
      base::BindOnce(
          &It2MeNativeMessagingHostLacros::OnSupportHostDetailsReceived,
          weak_factory_.GetWeakPtr()));
}

scoped_refptr<base::SingleThreadTaskRunner>
It2MeNativeMessagingHostLacros::task_runner() const {
  return task_runner_;
}

void It2MeNativeMessagingHostLacros::OnHostStateStarting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleHostStateChange(It2MeHostState::kStarting);
}

void It2MeNativeMessagingHostLacros::OnHostStateRequestedAccessCode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleHostStateChange(It2MeHostState::kRequestedAccessCode);
}

void It2MeNativeMessagingHostLacros::OnHostStateReceivedAccessCode(
    const std::string& access_code,
    base::TimeDelta lifetime) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;
  message.Set(kAccessCode, access_code);
  message.Set(kAccessCodeLifetime, static_cast<int>(lifetime.InSeconds()));
  HandleHostStateChange(It2MeHostState::kReceivedAccessCode,
                        std::move(message));
}

void It2MeNativeMessagingHostLacros::OnHostStateConnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleHostStateChange(It2MeHostState::kConnecting);
}

void It2MeNativeMessagingHostLacros::OnHostStateConnected(
    const std::string& remote_username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;
  message.Set(kClient, remote_username);
  HandleHostStateChange(It2MeHostState::kConnected, std::move(message));
}

void It2MeNativeMessagingHostLacros::OnHostStateDisconnected(
    const std::optional<std::string>& disconnect_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;
  if (disconnect_reason.has_value()) {
    message.Set(kDisconnectReason, *disconnect_reason);
  }
  HandleHostStateChange(It2MeHostState::kDisconnected, std::move(message));
}

void It2MeNativeMessagingHostLacros::OnNatPolicyChanged(
    mojom::NatPolicyStatePtr policy_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;
  message.Set(kMessageType, kNatPolicyChangedMessage);
  message.Set(kNatPolicyChangedMessageNatEnabled, policy_state->nat_enabled);
  message.Set(kNatPolicyChangedMessageRelayEnabled,
              policy_state->relay_enabled);
  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHostLacros::OnHostStateError(int64_t error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(error_code, 0);
  LOG_IF(WARNING, static_cast<int>(error_code) >=
                      static_cast<int>(protocol::ErrorCode::ERROR_CODE_MAX))
      << "|error_code| is greater than the max known error_code.";
  SendErrorAndExit(static_cast<protocol::ErrorCode>(error_code));
}

void It2MeNativeMessagingHostLacros::OnPolicyError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict response;
  response.Set(kMessageType, kPolicyErrorMessage);
  SendMessageToClient(std::move(response));
  client_->CloseChannel(std::string());
}

void It2MeNativeMessagingHostLacros::OnInvalidDomainError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleHostStateChange(It2MeHostState::kInvalidDomainError);
}

void It2MeNativeMessagingHostLacros::HandleHostStateChange(
    It2MeHostState state,
    base::Value::Dict message) {
  message.Set(kMessageType, kHostStateChangedMessage);

  switch (state) {
    case It2MeHostState::kStarting:
      message.Set(kState, kHostStateStarting);
      break;

    case It2MeHostState::kRequestedAccessCode:
      message.Set(kState, kHostStateRequestedAccessCode);
      break;

    case It2MeHostState::kReceivedAccessCode:
      message.Set(kState, kHostStateReceivedAccessCode);
      break;

    case It2MeHostState::kConnecting:
      message.Set(kState, kHostStateConnecting);
      break;

    case It2MeHostState::kConnected:
      message.Set(kState, kHostStateConnected);
      break;

    case It2MeHostState::kDisconnected:
      message.Set(kState, kHostStateDisconnected);
      break;

    case It2MeHostState::kInvalidDomainError:
      message.Set(kState, kHostStateDomainError);
      break;

    default:
      NOTREACHED();
  }

  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHostLacros::OnSupportHostDetailsReceived(
    mojom::SupportHostDetailsPtr host_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_details_ = std::move(host_details);

  if (hello_response_pending_) {
    hello_response_pending_ = false;
    int response_id = hello_response_id_;
    hello_response_id_ = kInvalidMessageId;
    ProcessHello(response_id);
  }
}

void It2MeNativeMessagingHostLacros::OnSupportSessionStarted(
    mojom::StartSupportSessionResponsePtr mojo_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_id = connect_response_id_;
  connect_response_id_ = kInvalidMessageId;
  if (mojo_response->is_support_session_error()) {
    SendErrorAndExit(SupportSessionErrorToProtocolError(
                         mojo_response->get_support_session_error()),
                     response_id);
    return;
  }

  support_host_observer_.Bind(std::move(mojo_response->get_observer()));

  base::Value::Dict response;
  response.Set(kMessageType, kConnectResponse);

  if (response_id != kInvalidMessageId) {
    response.Set(kMessageId, response_id);
  }

  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHostLacros::ProcessHello(int message_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (host_details_.is_null()) {
    // We haven't received the host details from ash so wait before responding.
    hello_response_pending_ = true;
    hello_response_id_ = message_id;
    return;
  }

  base::Value::Dict response;
  response.Set(kMessageType, kHelloResponse);
  if (message_id != kInvalidMessageId) {
    response.Set(kMessageId, message_id);
  }

  response.Set(kHostVersion, host_details_.get()->host_version);

  base::Value::List features;
  for (const auto& feature : host_details_.get()->supported_features) {
    features.Append(feature);
  }
  response.Set(kSupportedFeatures, std::move(features));
  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHostLacros::ProcessConnect(int message_id,
                                                    base::Value::Dict message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (message_id != kInvalidMessageId) {
    connect_response_id_ = message_id;
  }

  mojom::SupportSessionParamsPtr session_params =
      mojom::SupportSessionParams::New();

  const std::string* user_name = message.FindString(kUserName);
  if (!user_name) {
    SendErrorAndExit(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL, message_id);
    return;
  }
  session_params->user_name = *user_name;

  // The code to extract and forward the access_token needs to handle a couple
  // of conditions due to Lacros/Ash version skew. The M121 CRD host
  // implementation can handle `oauth_access_token` values which are raw or
  // prefixed by 'oauth2:' however Lacros can run on older versions of Ash which
  // only handle the prefixed variant. This compat code can be removed in M124
  // based on the current version skew policy.
  // This code also handles the case where an older webclient does not send the
  // kAccessToken field however that will resolve in a few weeks and the code
  // can safely be removed in M122.
  std::string access_token;
  const std::string* access_token_ptr = message.FindString(kAccessToken);
  const std::string* auth_service_with_token_ptr =
      message.FindString(kAuthServiceWithToken);
  if (access_token_ptr) {
    // TODO(b/309958013): Remove the prefix shim.
    access_token = "oauth2:" + *access_token_ptr;
  } else if (auth_service_with_token_ptr) {
    // kAuthServiceWithToken always starts with 'oauth2:'.
    access_token = *auth_service_with_token_ptr;
  }
  if (access_token.empty()) {
    SendErrorAndExit(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL, message_id);
    return;
  }
  session_params->oauth_access_token = std::move(access_token);

  const std::string* authorized_helper = message.FindString(kAuthorizedHelper);
  if (authorized_helper) {
    session_params->authorized_helper =
        gaia::CanonicalizeEmail(*authorized_helper);
    if (!IsValidEmailAddress(session_params->authorized_helper.value())) {
      SendErrorAndExit(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL, message_id);
      return;
    }
  }

  // TODO(joedow): Add the ability to toggle the RemoteCommand settings for
  // testing purposes. This should probably be encapsulated in a check that the
  // machine is in developer-mode and/or !NDEBUG.

  auto* lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::Remoting>()->StartSupportSession(
      std::move(session_params),
      base::BindOnce(&It2MeNativeMessagingHostLacros::OnSupportSessionStarted,
                     weak_factory_.GetWeakPtr()));
}

void It2MeNativeMessagingHostLacros::ProcessDisconnect(int message_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Resetting the observer will cause the host running in ash to disconnect.
  support_host_observer_.reset();

  // Since the mojo channel was disconnected above, we will no longer receive
  // events from ash-chrome (including the disconnected host state change).
  // Set the new state here to reflect that we've disconnected the host. This
  // will cause a message to be sent to the client so it can update its UI.
  HandleHostStateChange(It2MeHostState::kDisconnected);

  base::Value::Dict response;
  response.Set(kMessageType, kDisconnectResponse);

  if (message_id != kInvalidMessageId) {
    response.Set(kMessageId, message_id);
  }

  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHostLacros::SendMessageToClient(
    base::Value::Dict message) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  client_->PostMessageFromNativeHost(message_json);
}

void It2MeNativeMessagingHostLacros::SendErrorAndExit(
    const protocol::ErrorCode error_code,
    int message_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;

  message.Set(kMessageType, kErrorMessage);
  if (message_id != kInvalidMessageId) {
    message.Set(kMessageId, message_id);
  }
  message.Set(kErrorMessageCode, ErrorCodeToString(error_code));

  SendMessageToClient(std::move(message));

  // Trigger a host shutdown by sending an empty message.
  client_->CloseChannel(std::string());
}

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateIt2MeNativeMessagingHostForLacros(
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner) {
  return std::make_unique<It2MeNativeMessagingHostLacros>(std::move(ui_runner));
}

}  // namespace remoting
