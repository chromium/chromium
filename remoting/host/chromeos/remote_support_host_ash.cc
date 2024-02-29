// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remote_support_host_ash.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringize_macros.h"
#include "remoting/host/chromeos/browser_interop.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/session_storage.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"
#include "remoting/host/it2me/reconnect_params.h"
#include "remoting/host/policy_watcher.h"

namespace remoting {

namespace {

using remoting::features::kEnableCrdAdminRemoteAccessV2;

base::Value::Dict EnterpriseParamsToDict(
    const ChromeOsEnterpriseParams& params) {
  return base::Value::Dict()
      .Set(kSuppressUserDialogs, params.suppress_user_dialogs)
      .Set(kSuppressNotifications, params.suppress_notifications)
      .Set(kTerminateUponInput, params.terminate_upon_input)
      .Set(kCurtainLocalUserSession, params.curtain_local_user_session)
      .Set(kShowTroubleshootingTools, params.show_troubleshooting_tools)
      .Set(kAllowTroubleshootingTools, params.allow_troubleshooting_tools)
      .Set(kAllowReconnections, params.allow_reconnections)
      .Set(kAllowFileTransfer, params.allow_file_transfer);
}

ChromeOsEnterpriseParams EnterpriseParamsFromDict(
    const base::Value::Dict& dict) {
  return ChromeOsEnterpriseParams{
      .suppress_user_dialogs =
          dict.FindBool(kSuppressUserDialogs).value_or(false),
      .suppress_notifications =
          dict.FindBool(kSuppressNotifications).value_or(false),
      .terminate_upon_input =
          dict.FindBool(kTerminateUponInput).value_or(false),
      .curtain_local_user_session =
          dict.FindBool(kCurtainLocalUserSession).value_or(false),
      .show_troubleshooting_tools =
          dict.FindBool(kShowTroubleshootingTools).value_or(false),
      .allow_troubleshooting_tools =
          dict.FindBool(kAllowTroubleshootingTools).value_or(false),
      .allow_reconnections = dict.FindBool(kAllowReconnections).value_or(false),
      .allow_file_transfer = dict.FindBool(kAllowFileTransfer).value_or(false),
  };
}

base::Value::Dict SessionParamsToDict(
    const mojom::SupportSessionParams& params) {
  auto session_params = base::Value::Dict()
                            .Set(kUserName, params.user_name)
                            .Set(kAuthorizedHelper, *params.authorized_helper);

  return session_params;
}

mojom::SupportSessionParams SessionParamsFromDict(
    const base::Value::Dict& dict) {
  mojom::SupportSessionParams result;
  const std::string* user_name = dict.FindString(kUserName);
  if (user_name) {
    result.user_name = *user_name;
  } else {
    LOG(ERROR) << "SupportSessionParams missing field: " << kUserName;
  }

  const std::string* authorized_helper = dict.FindString(kAuthorizedHelper);
  if (authorized_helper) {
    result.authorized_helper = *authorized_helper;
  } else {
    LOG(ERROR) << "SupportSessionParams missing field: " << kAuthorizedHelper;
  }

  return result;
}

mojom::StartSupportSessionResponsePtr GetUnableToReconnectError() {
  // TODO(joedow): Add better error messages.
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
    const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
    StartSessionCallback callback) {
  StartSession(params, enterprise_params, std::nullopt, std::move(callback));
}

void RemoteSupportHostAsh::StartSession(
    const mojom::SupportSessionParams& params,
    const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
    const std::optional<ReconnectParams>& reconnect_params,
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
      base::BindOnce(&RemoteSupportHostAsh::OnHostStateConnected,
                     base::Unretained(this), params, enterprise_params),
      base::BindOnce(&RemoteSupportHostAsh::OnHostStateDisconnected,
                     base::Unretained(this)),
      base::BindOnce(&RemoteSupportHostAsh::OnSessionDisconnected,
                     base::Unretained(this)));
}

void RemoteSupportHostAsh::ReconnectToSession(SessionId session_id,
                                              const std::string& access_token,
                                              StartSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    std::move(callback).Run(GetUnableToReconnectError());
    return;
  }

  if (session_id != kEnterpriseSessionId) {
    LOG(ERROR) << "CRD: No reconnectable session found with id " << session_id;
    std::move(callback).Run(GetUnableToReconnectError());
    return;
  }

  LOG(INFO) << "CRD: Retrieving details for reconnectable session id:"
            << session_id;
  session_storage_->RetrieveSession(base::BindOnce(
      &RemoteSupportHostAsh::OnSessionRetrieved, weak_ptr_factory_.GetWeakPtr(),
      session_id, access_token, std::move(callback)));
}

void RemoteSupportHostAsh::OnSessionRetrieved(
    SessionId session_id,
    const std::string& access_token,
    StartSessionCallback callback,
    std::optional<base::Value::Dict> session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!session.has_value()) {
    LOG(ERROR) << "CRD: No reconnectable session found for id: " << session_id;
    std::move(callback).Run(GetUnableToReconnectError());
    return;
  }

  // Remove the stored session information now that we've read it, so we
  // do not keep it around forever.
  session_storage_->DeleteSession(base::DoNothing());

  auto session_params =
      SessionParamsFromDict(*session->EnsureDict(kSessionParamsDict));
  // DCHECK is added to detect cases where the access_token prefix is still
  // being used when it shouldn't as this will mess up the store/retrieve cycle.
  // TODO(b/309958013): Remove this DCHECK after M122.
  DCHECK(!access_token.starts_with("oauth2:"));
  session_params.oauth_access_token = access_token;

  LOG(INFO) << "CRD: Reconnectable session found - starting connection";
  StartSession(
      std::move(session_params),
      EnterpriseParamsFromDict(*session->EnsureDict(kEnterpriseParamsDict)),
      ReconnectParams::FromDict(*session->EnsureDict(kReconnectParamsDict)),
      std::move(callback));
}

// static
mojom::SupportHostDetailsPtr RemoteSupportHostAsh::GetHostDetails() {
  return mojom::SupportHostDetails::New(
      STRINGIZE(VERSION), std::vector<std::string>({kFeatureAccessTokenAuth,
                                                    kFeatureAuthorizedHelper}));
}

void RemoteSupportHostAsh::OnHostStateConnected(
    mojom::SupportSessionParams session_params,
    std::optional<ChromeOsEnterpriseParams> enterprise_params,
    std::optional<ReconnectParams> reconnect_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    return;
  }

  if (reconnect_params.has_value()) {
    CHECK(enterprise_params.has_value());
    CHECK(enterprise_params->allow_reconnections);

    LOG(INFO) << "CRD: Storing information for reconnectable session";
    session_storage_->StoreSession(
        base::Value::Dict()
            .Set(kSessionParamsDict, SessionParamsToDict(session_params))
            .Set(kEnterpriseParamsDict,
                 EnterpriseParamsToDict(*enterprise_params))
            .Set(kReconnectParamsDict,
                 ReconnectParams::ToDict(*reconnect_params)),
        base::DoNothing());
    return;
  }

  VLOG(3) << "CRD: Not a reconnectable session";
}

void RemoteSupportHostAsh::OnHostStateDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't allow reconnecting to the session if the client disconnects.
  LOG(INFO) << "Deleting reconnectable session info after client disconnect";
  session_storage_->DeleteSession(base::DoNothing());
  return;
}

void RemoteSupportHostAsh::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't allow reconnecting to the session if we explicitly disconnect the
  // session.
  LOG(INFO) << "Deleting reconnectable session info after host-side disconnect";
  session_storage_->DeleteSession(base::DoNothing());

  if (it2me_native_message_host_ash_) {
    // Do not access any instance members after |cleanup_callback_| is run as
    // this instance will be destroyed by running this.
    std::move(cleanup_callback_).Run();
  }
}

}  // namespace remoting
