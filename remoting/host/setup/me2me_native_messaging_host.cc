// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "net/base/network_interfaces.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/oauth_client.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/pin_hash.h"
#include "remoting/protocol/pairing_registry.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/elevated_native_messaging_host.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

#if BUILDFLAG(IS_WIN)
const int kElevatedHostTimeoutSeconds = 300;
#endif  // BUILDFLAG(IS_WIN)

// Features supported in addition to the base protocol.
const char* kSupportedFeatures[] = {
    "pairingRegistry",
    "oauthClient",
    "getRefreshTokenFromAuthCode",
#if BUILDFLAG(IS_APPLE)
    "it2mePermissionCheck",
#endif  // BUILDFLAG(IS_APPLE)
};

// Helper to extract the "config" part of a message as a base::Value::Dict.
// Returns nullptr on failure, and logs an error message.
std::optional<base::Value::Dict> ConfigDictionaryFromMessage(
    base::Value::Dict message) {
  if (base::Value::Dict* config_dict = message.FindDict("config")) {
    return std::move(*config_dict);
  }
  return std::nullopt;
}

}  // namespace

namespace remoting {

Me2MeNativeMessagingHost::Me2MeNativeMessagingHost(
    bool needs_elevation,
    intptr_t parent_window_handle,
    std::unique_ptr<ChromotingHostContext> host_context,
    scoped_refptr<DaemonController> daemon_controller,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    std::unique_ptr<OAuthClient> oauth_client)
    : needs_elevation_(needs_elevation),
#if BUILDFLAG(IS_WIN)
      parent_window_handle_(parent_window_handle),
#endif
      host_context_(std::move(host_context)),
      daemon_controller_(daemon_controller),
      pairing_registry_(pairing_registry),
      oauth_client_(std::move(oauth_client)) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

Me2MeNativeMessagingHost::~Me2MeNativeMessagingHost() {
  DCHECK(task_runner()->BelongsToCurrentThread());
}

void Me2MeNativeMessagingHost::OnMessage(const std::string& message) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  base::Value::Dict response;
  std::optional<base::Value> message_value = base::JSONReader::Read(message);
  if (!message_value || !message_value->is_dict()) {
    OnError("Received a message that's not a dictionary.");
    return;
  }

  base::Value::Dict& message_dict = message_value->GetDict();

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  if (const base::Value* id = message_dict.Find("id")) {
    response.Set("id", id->Clone());
  }

  const std::string* type = message_dict.FindString("type");
  if (!type) {
    OnError("'type' not found");
    return;
  }

  response.Set("type", *type + "Response");

  if (*type == "hello") {
    ProcessHello(std::move(message_dict), std::move(response));
  } else if (*type == "clearPairedClients") {
    ProcessClearPairedClients(std::move(message_dict), std::move(response));
  } else if (*type == "deletePairedClient") {
    ProcessDeletePairedClient(std::move(message_dict), std::move(response));
  } else if (*type == "getHostName") {
    ProcessGetHostName(std::move(message_dict), std::move(response));
  } else if (*type == "getPinHash") {
    ProcessGetPinHash(std::move(message_dict), std::move(response));
  } else if (*type == "generateKeyPair") {
    ProcessGenerateKeyPair(std::move(message_dict), std::move(response));
  } else if (*type == "updateDaemonConfig") {
    ProcessUpdateDaemonConfig(std::move(message_dict), std::move(response));
  } else if (*type == "getDaemonConfig") {
    ProcessGetDaemonConfig(std::move(message_dict), std::move(response));
  } else if (*type == "getPairedClients") {
    ProcessGetPairedClients(std::move(message_dict), std::move(response));
  } else if (*type == "getUsageStatsConsent") {
    ProcessGetUsageStatsConsent(std::move(message_dict), std::move(response));
  } else if (*type == "startDaemon") {
    ProcessStartDaemon(std::move(message_dict), std::move(response));
  } else if (*type == "stopDaemon") {
    ProcessStopDaemon(std::move(message_dict), std::move(response));
  } else if (*type == "getDaemonState") {
    ProcessGetDaemonState(std::move(message_dict), std::move(response));
  } else if (*type == "getHostClientId") {
    ProcessGetHostClientId(std::move(message_dict), std::move(response));
  } else if (*type == "getCredentialsFromAuthCode") {
    ProcessGetCredentialsFromAuthCode(std::move(message_dict),
                                      std::move(response), true);
  } else if (*type == "getRefreshTokenFromAuthCode") {
    ProcessGetCredentialsFromAuthCode(std::move(message_dict),
                                      std::move(response), false);
  } else if (*type == "it2mePermissionCheck") {
    ProcessIt2mePermissionCheck(std::move(message_dict), std::move(response));
  } else {
    OnError("Unsupported request type: " + *type);
  }
}

void Me2MeNativeMessagingHost::Start(Client* client) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  client_ = client;
  log_message_handler_ =
      std::make_unique<LogMessageHandler>(base::BindRepeating(
          &Me2MeNativeMessagingHost::SendMessageToClient, weak_ptr_));
}

scoped_refptr<base::SingleThreadTaskRunner>
Me2MeNativeMessagingHost::task_runner() const {
  return host_context_->ui_task_runner();
}

void Me2MeNativeMessagingHost::ProcessHello(base::Value::Dict message,
                                            base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("version", STRINGIZE(VERSION));
  base::Value::List supported_features_list;
  for (const char* feature : kSupportedFeatures) {
    supported_features_list.Append(feature);
  }
  response.Set("supportedFeatures", std::move(supported_features_list));
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessClearPairedClients(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (needs_elevation_) {
    if (DelegateToElevatedHost(std::move(message)) != DELEGATION_SUCCESS) {
      SendBooleanResult(std::move(response), false);
    }
    return;
  }

  if (pairing_registry_.get()) {
    pairing_registry_->ClearAllPairings(
        base::BindOnce(&Me2MeNativeMessagingHost::SendBooleanResult, weak_ptr_,
                       std::move(response)));
  } else {
    SendBooleanResult(std::move(response), false);
  }
}

void Me2MeNativeMessagingHost::ProcessDeletePairedClient(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (needs_elevation_) {
    if (DelegateToElevatedHost(std::move(message)) != DELEGATION_SUCCESS) {
      SendBooleanResult(std::move(response), false);
    }
    return;
  }

  std::string* client_id =
      message.FindString(protocol::PairingRegistry::kClientIdKey);
  if (!client_id) {
    OnError("'" + std::string(protocol::PairingRegistry::kClientIdKey) +
            "' string not found.");
    return;
  }

  if (pairing_registry_.get()) {
    pairing_registry_->DeletePairing(
        std::move(*client_id),
        base::BindOnce(&Me2MeNativeMessagingHost::SendBooleanResult, weak_ptr_,
                       std::move(response)));
  } else {
    SendBooleanResult(std::move(response), false);
  }
}

void Me2MeNativeMessagingHost::ProcessGetHostName(base::Value::Dict message,
                                                  base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("hostname", net::GetHostName());
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessGetPinHash(base::Value::Dict message,
                                                 base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  std::string* host_id = message.FindString("hostId");
  if (!host_id) {
    std::string message_json;
    base::JSONWriter::Write(message, &message_json);
    OnError("'hostId' not found: " + message_json);
    return;
  }
  std::string* pin = message.FindString("pin");
  if (!pin) {
    std::string message_json;
    base::JSONWriter::Write(message, &message_json);
    OnError("'pin' not found: " + message_json);
    return;
  }
  response.Set("hash", MakeHostPinHash(std::move(*host_id), std::move(*pin)));
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessGenerateKeyPair(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  response.Set("privateKey", key_pair->ToString());
  response.Set("publicKey", key_pair->GetPublicKey());
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessUpdateDaemonConfig(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (needs_elevation_) {
    DelegationResult result = DelegateToElevatedHost(std::move(message));
    switch (result) {
      case DELEGATION_SUCCESS:
        return;  // Result will be returned by elevated host.
      case DELEGATION_CANCELLED:
        SendAsyncResult(std::move(response),
                        DaemonController::RESULT_CANCELLED);
        return;
      default:
        SendAsyncResult(std::move(response), DaemonController::RESULT_FAILED);
        return;
    }
  }

  std::optional<base::Value::Dict> config_dict =
      ConfigDictionaryFromMessage(std::move(message));
  if (!config_dict) {
    OnError("'config' dictionary not found");
    return;
  }

  daemon_controller_->UpdateConfig(
      std::move(*config_dict),
      base::BindOnce(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                     std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessGetDaemonConfig(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  daemon_controller_->GetConfig(
      base::BindOnce(&Me2MeNativeMessagingHost::SendConfigResponse, weak_ptr_,
                     std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessGetPairedClients(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (pairing_registry_.get()) {
    pairing_registry_->GetAllPairings(
        base::BindOnce(&Me2MeNativeMessagingHost::SendPairedClientsResponse,
                       weak_ptr_, std::move(response)));
  } else {
    SendPairedClientsResponse(std::move(response),
                              /*pairings=*/base::Value::List());
  }
}

void Me2MeNativeMessagingHost::ProcessGetUsageStatsConsent(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  daemon_controller_->GetUsageStatsConsent(
      base::BindOnce(&Me2MeNativeMessagingHost::SendUsageStatsConsentResponse,
                     weak_ptr_, std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessStartDaemon(base::Value::Dict message,
                                                  base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (needs_elevation_) {
    DelegationResult result = DelegateToElevatedHost(std::move(message));
    switch (result) {
      case DELEGATION_SUCCESS:
        return;  // Result will be returned by elevated host.
      case DELEGATION_CANCELLED:
        SendAsyncResult(std::move(response),
                        DaemonController::RESULT_CANCELLED);
        return;
      default:
        SendAsyncResult(std::move(response), DaemonController::RESULT_FAILED);
        return;
    }
  }

  std::optional<bool> consent = message.FindBool("consent");
  if (!consent) {
    OnError("'consent' not found.");
    return;
  }

  std::optional<base::Value::Dict> config_dict =
      ConfigDictionaryFromMessage(std::move(message));
  if (!config_dict) {
    OnError("'config' dictionary not found");
    return;
  }

  daemon_controller_->SetConfigAndStart(
      std::move(*config_dict), *consent,
      base::BindOnce(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                     std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessStopDaemon(base::Value::Dict message,
                                                 base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (needs_elevation_) {
    DelegationResult result = DelegateToElevatedHost(std::move(message));
    switch (result) {
      case DELEGATION_SUCCESS:
        return;  // Result will be returned by elevated host.
      case DELEGATION_CANCELLED:
        SendAsyncResult(std::move(response),
                        DaemonController::RESULT_CANCELLED);
        return;
      default:
        SendAsyncResult(std::move(response), DaemonController::RESULT_FAILED);
        return;
    }
  }

  daemon_controller_->Stop(
      base::BindOnce(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                     std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessGetDaemonState(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  DaemonController::State state = daemon_controller_->GetState();
  switch (state) {
    case DaemonController::STATE_NOT_IMPLEMENTED:
      response.Set("state", "NOT_IMPLEMENTED");
      break;
    case DaemonController::STATE_STOPPED:
      response.Set("state", "STOPPED");
      break;
    case DaemonController::STATE_STARTING:
      response.Set("state", "STARTING");
      break;
    case DaemonController::STATE_STARTED:
      response.Set("state", "STARTED");
      break;
    case DaemonController::STATE_STOPPING:
      response.Set("state", "STOPPING");
      break;
    case DaemonController::STATE_UNKNOWN:
      response.Set("state", "UNKNOWN");
      break;
  }
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessGetHostClientId(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("clientId", google_apis::GetOAuth2ClientID(
                               google_apis::CLIENT_REMOTING_HOST));
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::ProcessGetCredentialsFromAuthCode(
    base::Value::Dict message,
    base::Value::Dict response,
    bool need_user_email) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  std::string* auth_code = message.FindString("authorizationCode");
  if (!auth_code) {
    OnError("'authorizationCode' string not found.");
    return;
  }

  // Pass an empty redirect_uri value since it's not needed when exchanging
  // robot auth codes. See b/231442487 for more details.
  std::string redirect_uri;
  gaia::OAuthClientInfo oauth_client_info = {
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST),
      redirect_uri};

  oauth_client_->GetCredentialsFromAuthCode(
      oauth_client_info, std::move(*auth_code), need_user_email,
      base::BindOnce(&Me2MeNativeMessagingHost::SendCredentialsResponse,
                     weak_ptr_, std::move(response)));
}

void Me2MeNativeMessagingHost::ProcessIt2mePermissionCheck(
    base::Value::Dict message,
    base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  daemon_controller_->CheckPermission(
      /* it2me */ true,
      base::BindOnce(&Me2MeNativeMessagingHost::SendBooleanResult, weak_ptr_,
                     std::move(response)));
}

void Me2MeNativeMessagingHost::SendConfigResponse(
    base::Value::Dict response,
    std::optional<base::Value::Dict> config) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (config) {
    response.Set("config", std::move(*config));
  } else {
    response.Set("config", base::Value());
  }
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendPairedClientsResponse(
    base::Value::Dict response,
    base::Value::List pairings) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("pairedClients", std::move(pairings));
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendUsageStatsConsentResponse(
    base::Value::Dict response,
    const DaemonController::UsageStatsConsent& consent) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("supported", consent.supported);
  response.Set("allowed", consent.allowed);
  response.Set("setByPolicy", consent.set_by_policy);
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendAsyncResult(
    base::Value::Dict response,
    DaemonController::AsyncResult result) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  switch (result) {
    case DaemonController::RESULT_OK:
      response.Set("result", "OK");
      break;
    case DaemonController::RESULT_FAILED:
      response.Set("result", "FAILED");
      break;
    case DaemonController::RESULT_CANCELLED:
      response.Set("result", "CANCELLED");
      break;
  }
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendBooleanResult(base::Value::Dict response,
                                                 bool result) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response.Set("result", result);
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendCredentialsResponse(
    base::Value::Dict response,
    const std::string& user_email,
    const std::string& refresh_token) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (!user_email.empty()) {
    response.Set("userEmail", user_email);
  }
  response.Set("refreshToken", refresh_token);
  SendMessageToClient(std::move(response));
}

void Me2MeNativeMessagingHost::SendMessageToClient(
    base::Value::Dict message) const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  client_->PostMessageFromNativeHost(message_json);
}

void Me2MeNativeMessagingHost::OnError(const std::string& error_message) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (!error_message.empty()) {
    LOG(ERROR) << error_message;
  }

  // Trigger a host shutdown by sending an empty message.
  client_->CloseChannel(std::string());
}

#if BUILDFLAG(IS_WIN)

Me2MeNativeMessagingHost::DelegationResult
Me2MeNativeMessagingHost::DelegateToElevatedHost(base::Value::Dict message) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(needs_elevation_);

  if (!elevated_host_) {
    elevated_host_ = std::make_unique<ElevatedNativeMessagingHost>(
        base::CommandLine::ForCurrentProcess()->GetProgram(),
        parent_window_handle_,
        /*elevate_process=*/true, base::Seconds(kElevatedHostTimeoutSeconds),
        client_);
  }

  ProcessLaunchResult result = elevated_host_->EnsureElevatedHostCreated();
  if (result == PROCESS_LAUNCH_RESULT_SUCCESS) {
    elevated_host_->SendMessage(message);
  }

  switch (result) {
    case PROCESS_LAUNCH_RESULT_SUCCESS:
      return DELEGATION_SUCCESS;
    case PROCESS_LAUNCH_RESULT_CANCELLED:
      return DELEGATION_CANCELLED;
    case PROCESS_LAUNCH_RESULT_FAILED:
      return DELEGATION_FAILED;
  }
}

#else  // BUILDFLAG(IS_WIN)

Me2MeNativeMessagingHost::DelegationResult
Me2MeNativeMessagingHost::DelegateToElevatedHost(base::Value::Dict message) {
  NOTREACHED();
}

#endif  // !BUILDFLAG(IS_WIN)

}  // namespace remoting
