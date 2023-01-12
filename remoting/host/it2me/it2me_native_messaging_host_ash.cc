// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/host/policy_watcher.h"

namespace remoting {

namespace {

bool ShouldSuppressNotifications(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params) {
  if (enterprise_params) {
    return enterprise_params.value().suppress_notifications;
  }

  // On non-debug builds, do not allow setting this value through the Mojom
  // API.
#if !defined(NDEBUG)
  return params.suppress_notifications;
#else
  return false;
#endif
}

bool ShouldSuppressUserDialog(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params) {
  if (enterprise_params) {
    return enterprise_params.value().suppress_user_dialogs;
  }

  // On non-debug builds, do not allow setting this value through the Mojom
  // API.
#if !defined(NDEBUG)
  return params.suppress_user_dialogs;
#else
  return false;
#endif
}

bool ShouldTerminateUponInput(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params) {
  if (enterprise_params) {
    return enterprise_params.value().terminate_upon_input;
  }

  // On non-debug builds, do not allow setting this value through the Mojom
  // API.
#if !defined(NDEBUG)
  return params.terminate_upon_input;
#else
  return false;
#endif
}

bool ShouldCurtainLocalUserSession(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params) {
  if (!base::FeatureList::IsEnabled(features::kEnableCrdAdminRemoteAccess)) {
    return false;
  }

  if (enterprise_params) {
    return enterprise_params.value().curtain_local_user_session;
  }

  // On non-debug builds, do not allow setting this value through the Mojom
  // API.
#if !defined(NDEBUG)
  return params.curtain_local_user_session;
#else
  return false;
#endif
}

}  // namespace

It2MeNativeMessageHostAsh::It2MeNativeMessageHostAsh() = default;
It2MeNativeMessageHostAsh::~It2MeNativeMessageHostAsh() = default;

mojo::PendingReceiver<mojom::SupportHostObserver>
It2MeNativeMessageHostAsh::Start(
    std::unique_ptr<ChromotingHostContext> context,
    std::unique_ptr<PolicyWatcher> policy_watcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!native_message_host_);

  // Create the remote IPC channel before starting the NMH so any errors are
  // queued for sending once the receiver end of the channel is bound.
  mojo::PendingReceiver<mojom::SupportHostObserver> observer =
      remote_.BindNewPipeAndPassReceiver();

  remote_.set_disconnect_handler(base::BindOnce(
      &It2MeNativeMessageHostAsh::Disconnect, base::Unretained(this)));

  std::unique_ptr<It2MeHostFactory> host_factory(new It2MeHostFactory());
  native_message_host_ = std::make_unique<It2MeNativeMessagingHost>(
      /*needs_elevation=*/false, std::move(policy_watcher), std::move(context),
      std::move(host_factory));
  native_message_host_->Start(this);

  return observer;
}

void It2MeNativeMessageHostAsh::Connect(
    mojom::SupportSessionParamsPtr params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
    base::OnceClosure connected_callback,
    base::OnceClosure disconnected_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(native_message_host_);
  DCHECK(!connected_callback_);
  DCHECK(!disconnected_callback_);

  connected_callback_ = std::move(connected_callback);
  disconnected_callback_ = std::move(disconnected_callback);

  base::Value::Dict message;
  message.Set(kMessageType, kConnectMessage);

  message.Set(kUserName, params->user_name);
  message.Set(kAuthServiceWithToken, params->oauth_access_token);
  message.Set(kSuppressUserDialogs,
              ShouldSuppressUserDialog(*params, enterprise_params));
  message.Set(kSuppressNotifications,
              ShouldSuppressNotifications(*params, enterprise_params));
  message.Set(kTerminateUponInput,
              ShouldTerminateUponInput(*params, enterprise_params));
  message.Set(kCurtainLocalUserSession,
              ShouldCurtainLocalUserSession(*params, enterprise_params));
  message.Set(kIsEnterpriseAdminUser, enterprise_params.has_value());
  if (params->authorized_helper.has_value()) {
    message.Set(kAuthorizedHelper, *params->authorized_helper);
  }

  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  native_message_host_->OnMessage(message_json);
}

void It2MeNativeMessageHostAsh::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict message;
  message.Set(kMessageType, kDisconnectMessage);

  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  native_message_host_->OnMessage(message_json);

  // Notify the owner that the host has been disconnected.  This will result in
  // the destruction of this object so do not access member variables after this
  // callback is run.
  std::move(disconnected_callback_).Run();
}

void It2MeNativeMessageHostAsh::PostMessageFromNativeHost(
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string type;
  base::Value::Dict contents;
  if (!ParseNativeMessageJson(message, type, contents)) {
    CloseChannel(std::string());
    return;
  }

  if (type.empty()) {
    LOG(ERROR) << "'type' not found in request.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  if (type == kConnectResponse) {
    HandleConnectResponse();
  } else if (type == kDisconnectResponse) {
    HandleDisconnectResponse();
  } else if (type == kIncomingIqResponse) {
    // These responses do not need to be handled as the Lacros NMH sends a
    // response when the request message is first received.
  } else if (type == kHostStateChangedMessage) {
    HandleHostStateChangeMessage(std::move(contents));
  } else if (type == kNatPolicyChangedMessage) {
    HandleNatPolicyChangedMessage(std::move(contents));
  } else if (type == kPolicyErrorMessage) {
    HandlePolicyErrorMessage(std::move(contents));
  } else if (type == kErrorMessage) {
    HandleErrorMessage(std::move(contents));
  } else {
    LOG(ERROR) << "Unsupported message type: " << type;
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
  }
}

void It2MeNativeMessageHostAsh::CloseChannel(const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG_IF(ERROR, !error_message.empty())
      << "CloseChannel called with error: " << error_message;
  Disconnect();
}

void It2MeNativeMessageHostAsh::HandleConnectResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(connected_callback_).Run();
}

void It2MeNativeMessageHostAsh::HandleDisconnectResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_->OnHostStateDisconnected(ErrorCodeToString(protocol::ErrorCode::OK));
}

void It2MeNativeMessageHostAsh::HandleHostStateChangeMessage(
    base::Value::Dict message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string* new_state = message.FindString(kState);
  if (!new_state) {
    LOG(ERROR) << "Missing |" << kState << "| value in message.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  if (*new_state == kHostStateStarting) {
    remote_->OnHostStateStarting();
  } else if (*new_state == kHostStateDisconnected) {
    const std::string* disconnect_reason =
        message.FindString(kDisconnectReason);
    remote_->OnHostStateDisconnected(
        disconnect_reason ? *disconnect_reason
                          : ErrorCodeToString(protocol::ErrorCode::OK));
  } else if (*new_state == kHostStateRequestedAccessCode) {
    remote_->OnHostStateRequestedAccessCode();
  } else if (*new_state == kHostStateReceivedAccessCode) {
    const std::string* access_code = message.FindString(kAccessCode);
    if (!access_code) {
      LOG(ERROR) << "Missing |" << kAccessCode << "| value in message.";
      CloseChannel(
          ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
      return;
    }
    absl::optional<int> access_code_lifetime =
        message.FindInt(kAccessCodeLifetime);
    if (!access_code_lifetime) {
      LOG(ERROR) << "Missing |" << kAccessCodeLifetime << "| value in message.";
      CloseChannel(
          ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
      return;
    }
    remote_->OnHostStateReceivedAccessCode(
        *access_code, base::Seconds(access_code_lifetime.value()));
  } else if (*new_state == kHostStateConnecting) {
    remote_->OnHostStateConnecting();
  } else if (*new_state == kHostStateConnected) {
    const std::string* remote_username = message.FindString(kClient);
    if (!remote_username) {
      LOG(ERROR) << "Missing |" << kClient << "| value in message.";
      CloseChannel(
          ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
      return;
    }
    remote_->OnHostStateConnected(*remote_username);
  } else if (*new_state == kHostStateError) {
    const std::string* error_code_string =
        message.FindString(kErrorMessageCode);
    if (!error_code_string) {
      LOG(ERROR) << "Missing |" << kErrorMessageCode << "| value in message.";
      CloseChannel(
          ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
      return;
    }

    protocol::ErrorCode error_code;
    if (!ParseErrorCode(*error_code_string, &error_code)) {
      LOG(ERROR) << "Invalid |" << kErrorMessageCode << "| value "
                 << *error_code_string << "in message.";
      CloseChannel(
          ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
      return;
    }
    remote_->OnHostStateError(error_code);
  } else if (*new_state == kHostStateDomainError) {
    remote_->OnInvalidDomainError();
  } else {
    NOTREACHED() << "Unknown state: " << *new_state;
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }
}

void It2MeNativeMessageHostAsh::HandleNatPolicyChangedMessage(
    base::Value::Dict message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<bool> nat_enabled =
      message.FindBool(kNatPolicyChangedMessageNatEnabled);
  if (!nat_enabled.has_value()) {
    LOG(ERROR) << "Missing |" << kNatPolicyChangedMessageNatEnabled
               << "| value in message.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  absl::optional<bool> relay_enabled =
      message.FindBool(kNatPolicyChangedMessageRelayEnabled);
  if (!nat_enabled.has_value()) {
    LOG(ERROR) << "Missing |" << kNatPolicyChangedMessageRelayEnabled
               << "| value in message.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  mojom::NatPolicyStatePtr nat_policy = mojom::NatPolicyState::New();
  nat_policy->nat_enabled = nat_enabled.value();
  nat_policy->relay_enabled = relay_enabled.value();
  remote_->OnNatPolicyChanged(std::move(nat_policy));
}

void It2MeNativeMessageHostAsh::HandlePolicyErrorMessage(
    base::Value::Dict message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_->OnPolicyError();
}

void It2MeNativeMessageHostAsh::HandleErrorMessage(base::Value::Dict message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string* error_code_string = message.FindString(kErrorMessageCode);
  if (!error_code_string) {
    LOG(ERROR) << "Missing |" << kErrorMessageCode << "| value in message.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  protocol::ErrorCode error_code;
  if (!ParseErrorCode(*error_code_string, &error_code)) {
    LOG(ERROR) << "Invalid |" << kErrorMessageCode << "| value "
               << *error_code_string << "in message.";
    CloseChannel(ErrorCodeToString(protocol::ErrorCode::INCOMPATIBLE_PROTOCOL));
    return;
  }

  remote_->OnHostStateError(error_code);
}

}  // namespace remoting
