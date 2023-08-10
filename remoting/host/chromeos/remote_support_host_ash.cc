// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remote_support_host_ash.h"

#include <utility>

#include <stddef.h>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/stringize_macros.h"
#include "remoting/host/chromeos/browser_interop.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/session_storage.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/connection_details.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"
#include "remoting/host/policy_watcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

namespace {

using remoting::features::kEnableCrdAdminRemoteAccessV2;

constexpr char kEnterpriseParamsDictKey[] = "enterprise-params";
constexpr char kSuppressUserDialogsKey[] = "suppress-user-dialogs";
constexpr char kSuppressNotificationsKey[] = "suppress-notifications";
constexpr char kTerminateUponInputKey[] = "terminate-upon-input";
constexpr char kCurtainLocalUserSessionKey[] = "curtain-local-user-session";
constexpr char kShowTroubleshootingToolsKey[] = "show-troubleshooting-tools";
constexpr char kAllowTroubleshootingToolsKey[] = "allow-troubleshooting-tools";
constexpr char kAllowReconnections[] = "allow-reconnections";
constexpr char kAllowFileTransfer[] = "allow-file-transfer";

constexpr char kSessionParamsDictKey[] = "session-params";
constexpr char kUserNameKey[] = "user-name";
constexpr char kOauthTokenKey[] = "oauth-token";

constexpr char kReconnectParamsDictKey[] = "reconnect-params";
constexpr char kRemoteUsernameKey[] = "remote-username";

base::Value::Dict EnterpriseParamsToDict(
    const ChromeOsEnterpriseParams& params) {
  return base::Value::Dict()
      .Set(kSuppressUserDialogsKey, params.suppress_user_dialogs)
      .Set(kSuppressNotificationsKey, params.suppress_notifications)
      .Set(kTerminateUponInputKey, params.terminate_upon_input)
      .Set(kCurtainLocalUserSessionKey, params.curtain_local_user_session)
      .Set(kShowTroubleshootingToolsKey, params.show_troubleshooting_tools)
      .Set(kAllowTroubleshootingToolsKey, params.allow_troubleshooting_tools)
      .Set(kAllowReconnections, params.allow_reconnections)
      .Set(kAllowFileTransfer, params.allow_file_transfer);
}

ChromeOsEnterpriseParams EnterpriseParamsFromDict(
    const base::Value::Dict& dict) {
  return ChromeOsEnterpriseParams{
      .suppress_user_dialogs = dict.FindBool(kSuppressUserDialogsKey).value(),
      .suppress_notifications =
          dict.FindBool(kSuppressNotificationsKey).value(),
      .terminate_upon_input = dict.FindBool(kTerminateUponInputKey).value(),
      .curtain_local_user_session =
          dict.FindBool(kCurtainLocalUserSessionKey).value(),
      .show_troubleshooting_tools =
          dict.FindBool(kShowTroubleshootingToolsKey).value(),
      .allow_troubleshooting_tools =
          dict.FindBool(kAllowTroubleshootingToolsKey).value(),
      .allow_reconnections = dict.FindBool(kAllowReconnections).value(),
      .allow_file_transfer = dict.FindBool(kAllowFileTransfer).value(),
  };
}

base::Value::Dict SessionParamsToDict(
    const mojom::SupportSessionParams& params) {
  return base::Value::Dict()
      .Set(kUserNameKey, params.user_name)
      .Set(kOauthTokenKey, params.oauth_access_token);
}

mojom::SupportSessionParams SessionParamsFromDict(
    const base::Value::Dict& dict) {
  mojom::SupportSessionParams result;
  result.user_name = *dict.FindString(kUserNameKey);
  result.oauth_access_token = *dict.FindString(kOauthTokenKey);
  return result;
}

base::Value::Dict ConnectionDetailsToDict(const ConnectionDetails& details) {
  return base::Value::Dict().Set(kRemoteUsernameKey, details.remote_username);
}

ConnectionDetails ConnectionDetailsFromDict(const base::Value::Dict& dict) {
  return {.remote_username = *dict.FindString(kRemoteUsernameKey)};
}

mojom::StartSupportSessionResponsePtr GetUnableToReconnectError() {
  return mojom::StartSupportSessionResponse::NewSupportSessionError(
      mojom::StartSupportSessionError::kUnknown);
}

}  // namespace

RemoteSupportHostAsh::RemoteSupportHostAsh(base::OnceClosure cleanup_callback,
                                           SessionStorage& session_storage)
    : RemoteSupportHostAsh(std::make_unique<It2MeHostFactory>(),
                           base::MakeRefCounted<BrowserInterop>(),
                           session_storage,
                           std::move(cleanup_callback)) {}

RemoteSupportHostAsh::RemoteSupportHostAsh(
    std::unique_ptr<It2MeHostFactory> host_factory,
    scoped_refptr<BrowserInterop> browser_interop,
    SessionStorage& session_storage,
    base::OnceClosure cleanup_callback)
    : host_factory_(std::move(host_factory)),
      browser_interop_(browser_interop),
      session_storage_(session_storage),
      cleanup_callback_(std::move(cleanup_callback)) {}

RemoteSupportHostAsh::~RemoteSupportHostAsh() = default;

void RemoteSupportHostAsh::StartSession(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
    StartSessionCallback callback) {
  StartSession(params, enterprise_params, absl::nullopt, std::move(callback));
}

void RemoteSupportHostAsh::StartSession(
    const mojom::SupportSessionParams& params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
    const absl::optional<ConnectionDetails>& reconnect_params,
    StartSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure there is at most one active remote support connection.
  // Since we are initiating the disconnect, don't run the cleanup callback.
  if (it2me_native_message_host_ash_) {
    auto temp = std::move(it2me_native_message_host_ash_);
    temp->Disconnect();
  }

  it2me_native_message_host_ash_ =
      std::make_unique<It2MeNativeMessageHostAsh>(host_factory_->Clone());

  mojo::PendingReceiver<mojom::SupportHostObserver> pending_receiver =
      it2me_native_message_host_ash_->Start(
          browser_interop_->CreateChromotingHostContext(),
          browser_interop_->CreatePolicyWatcher());

  mojom::StartSupportSessionResponsePtr response =
      mojom::StartSupportSessionResponse::NewObserver(
          std::move(pending_receiver));

  it2me_native_message_host_ash_->Connect(
      params, enterprise_params, reconnect_params,
      base::BindOnce(std::move(callback), std::move(response)),
      base::BindOnce(&RemoteSupportHostAsh::OnClientConnected,
                     base::Unretained(this), params, enterprise_params),
      base::BindOnce(&RemoteSupportHostAsh::OnSessionDisconnected,
                     base::Unretained(this)));
}

void RemoteSupportHostAsh::ReconnectToSession(SessionId session_id,
                                              StartSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    std::move(callback).Run(GetUnableToReconnectError());
    return;
  }

  if (session_id != kEnterpriseSessionId) {
    LOG(ERROR) << "No reconnectable session found with id " << session_id;
    std::move(callback).Run(GetUnableToReconnectError());
    return;
  }

  session_storage_->RetrieveSession(base::BindOnce(
      [](base::WeakPtr<RemoteSupportHostAsh> self,
         StartSessionCallback callback,
         absl::optional<base::Value::Dict> session) {
        if (!self) {
          return;
        }

        if (!session.has_value()) {
          LOG(ERROR) << "No reconnectable session found";
          std::move(callback).Run(GetUnableToReconnectError());
          return;
        }

        LOG(INFO) << "Reconnectable session found - starting connection";
        self->StartSession(
            SessionParamsFromDict(*session->EnsureDict(kSessionParamsDictKey)),
            EnterpriseParamsFromDict(
                *session->EnsureDict(kEnterpriseParamsDictKey)),
            ConnectionDetailsFromDict(
                *session->EnsureDict(kReconnectParamsDictKey)),
            std::move(callback));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
mojom::SupportHostDetailsPtr RemoteSupportHostAsh::GetHostDetails() {
  return mojom::SupportHostDetails::New(
      STRINGIZE(VERSION), std::vector<std::string>({kFeatureAccessTokenAuth,
                                                    kFeatureAuthorizedHelper}));
}

void RemoteSupportHostAsh::OnClientConnected(
    mojom::SupportSessionParams params,
    absl::optional<ChromeOsEnterpriseParams> enterprise_params,
    ConnectionDetails details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    return;
  }

  if (enterprise_params.has_value() && enterprise_params->allow_reconnections) {
    LOG(INFO) << "Storing information for reconnectable CRD session";
    session_storage_->StoreSession(
        base::Value::Dict()
            .Set(kReconnectParamsDictKey, ConnectionDetailsToDict(details))
            .Set(kSessionParamsDictKey, SessionParamsToDict(params))
            .Set(kEnterpriseParamsDictKey,
                 EnterpriseParamsToDict(*enterprise_params)),
        base::DoNothing());
  }
}

void RemoteSupportHostAsh::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (it2me_native_message_host_ash_) {
    // Do not access any instance members after |cleanup_callback_| is run as
    // this instance will be destroyed by running this.
    std::move(cleanup_callback_).Run();
  }
}

}  // namespace remoting
