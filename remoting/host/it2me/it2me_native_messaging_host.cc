// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/policy_constants.h"
#include "net/base/url_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_helpers.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/remoting_register_support_host_request.h"
#include "remoting/host/xmpp_register_support_host_request.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/signaling/delegating_signal_strategy.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/remoting_log_to_server.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/xmpp_log_to_server.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "base/files/file_path.h"

#include "remoting/host/win/elevated_native_messaging_host.h"
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

using protocol::ErrorCode;

namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kBaseHostBinaryName[] =
    FILE_PATH_LITERAL("remote_assistance_host.exe");
const base::FilePath::CharType kElevatedHostBinaryName[] =
    FILE_PATH_LITERAL("remote_assistance_host_uiaccess.exe");
#endif  // BUILDFLAG(IS_WIN)

constexpr char kAnonymousUserName[] = "anonymous_user";

// Helper functions to run |callback| asynchronously on the correct thread
// using |task_runner|.
void PolicyUpdateCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    remoting::PolicyWatcher::PolicyUpdatedCallback callback,
    base::Value::Dict policies) {
  DCHECK(callback);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(callback, std::move(policies)));
}

void PolicyErrorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    remoting::PolicyWatcher::PolicyErrorCallback callback) {
  DCHECK(callback);
  task_runner->PostTask(FROM_HERE, callback);
}

// This function checks the email address provided to see if it is properly
// formatted. It does not validate the username or domain sections.
// TODO(joedow): Move to a shared location.
bool IsValidEmailAddress(const std::string& email) {
  return base::SplitString(email, "@", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL)
             .size() == 2U;
}

}  // namespace

It2MeNativeMessagingHost::It2MeNativeMessagingHost(
    bool is_process_elevated,
    std::unique_ptr<PolicyWatcher> policy_watcher,
    std::unique_ptr<ChromotingHostContext> context,
    std::unique_ptr<It2MeHostFactory> factory)
    : is_process_elevated_(is_process_elevated),
      host_context_(std::move(context)),
      factory_(std::move(factory)),
      policy_watcher_(std::move(policy_watcher)) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // The policy watcher runs on the |file_task_runner| but we want to run the
  // callbacks on |task_runner| so we use a shim to post them to it.
  PolicyWatcher::PolicyUpdatedCallback update_callback =
      base::BindRepeating(&It2MeNativeMessagingHost::OnPolicyUpdate, weak_ptr_);
  PolicyWatcher::PolicyErrorCallback error_callback =
      base::BindRepeating(&It2MeNativeMessagingHost::OnPolicyError, weak_ptr_);
  policy_watcher_->StartWatching(
      base::BindRepeating(&PolicyUpdateCallback, task_runner(),
                          update_callback),
      base::BindRepeating(&PolicyErrorCallback, task_runner(), error_callback));
}

It2MeNativeMessagingHost::~It2MeNativeMessagingHost() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = nullptr;
  }
}

void It2MeNativeMessagingHost::OnMessage(const std::string& message) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  std::string type;
  base::Value::Dict request;
  if (!ParseNativeMessageJson(message, type, request)) {
    client_->CloseChannel(std::string());
    return;
  }

  absl::optional<base::Value::Dict> response =
      CreateNativeMessageResponse(request);
  if (!response.has_value()) {
    SendErrorAndExit(base::Value::Dict(), ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  if (type == kHelloMessage) {
    ProcessHello(std::move(request), std::move(*response));
  } else if (type == kConnectMessage) {
    ProcessConnect(std::move(request), std::move(*response));
  } else if (type == kDisconnectMessage) {
    ProcessDisconnect(std::move(request), std::move(*response));
  } else if (type == kIncomingIqMessage) {
    ProcessIncomingIq(std::move(request), std::move(*response));
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
    SendErrorAndExit(std::move(request), ErrorCode::INCOMPATIBLE_PROTOCOL);
  }
}

void It2MeNativeMessagingHost::Start(Client* client) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  client_ = client;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  log_message_handler_ = std::make_unique<LogMessageHandler>(
      base::BindRepeating(&It2MeNativeMessagingHost::SendMessageToClient,
                          base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void It2MeNativeMessagingHost::SendMessageToClient(
    base::Value::Dict message) const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  client_->PostMessageFromNativeHost(message_json);
}

void It2MeNativeMessagingHost::ProcessHello(base::Value::Dict message,
                                            base::Value::Dict response) const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // No need to forward to the elevated process since no internal state is set.

  base::Value::List features;
  features.Append(kFeatureAccessTokenAuth);
  features.Append(kFeatureDelegatedSignaling);
  features.Append(kFeatureAuthorizedHelper);

  ProcessNativeMessageHelloResponse(response, std::move(features));
  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::ProcessConnect(base::Value::Dict message,
                                              base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (!policy_received_) {
    DCHECK(!pending_connect_);
    pending_connect_ =
        base::BindOnce(&It2MeNativeMessagingHost::ProcessConnect, weak_ptr_,
                       std::move(message), std::move(response));
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Requests that the support host is launched with UiAccess on Windows.
  // This value, in conjuction with the platform policy, is used to determine
  // if an elevated host should be used.
  bool use_elevated_host = message.FindBool(kUseElevatedHost).value_or(false);

  if (!is_process_elevated_) {
    auto allow_elevation_policy = GetAllowElevatedHostPolicyValue();
    // Honor the platform policy value if it is set, otherwise use the value
    // provided through the native messaging host.
    use_elevated_host_ = allow_elevation_policy.value_or(use_elevated_host);
  }
#else
  CHECK(!is_process_elevated_) << "Unexpected value for this platform";
#endif

  if (use_elevated_host_) {
    // Attempt to pass the current message to the elevated process.  This method
    // will spin up the elevated process if it is not already running.  On
    // success, the elevated process will process the message and respond.
    // If the process cannot be started or message passing fails, then return an
    // error to the message sender.
    if (!DelegateToElevatedHost(std::move(message))) {
      LOG(ERROR) << "Failed to send message to elevated host.";
      SendErrorAndExit(std::move(response), ErrorCode::ELEVATION_ERROR);
    }
    return;
  }

  if (it2me_host_.get()) {
    LOG(ERROR) << "Connect can be called only when disconnected.";
    SendErrorAndExit(std::move(response), ErrorCode::UNKNOWN_ERROR);
    return;
  }

  bool use_signaling_proxy =
      message.FindBool(kUseSignalingProxy).value_or(false);

  std::string username;
  const std::string* username_from_message = message.FindString(kUserName);
  if (username_from_message) {
    username = *username_from_message;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  bool suppress_user_dialogs =
      message.FindBool(kSuppressUserDialogs).value_or(false);
  bool suppress_notifications =
      message.FindBool(kSuppressNotifications).value_or(false);
  bool terminate_upon_input =
      message.FindBool(kTerminateUponInput).value_or(false);
  bool curtain_local_user_session =
      message.FindBool(kCurtainLocalUserSession).value_or(false);
#endif

  bool is_enterprise_admin_user =
      message.FindBool(kIsEnterpriseAdminUser).value_or(false);

  std::string authorized_helper;
  const std::string* authorized_helper_value =
      message.FindString(kAuthorizedHelper);
  if (authorized_helper_value) {
    authorized_helper = *authorized_helper_value;
    if (!IsValidEmailAddress(authorized_helper)) {
      LOG(ERROR) << "Invalid authorized_helper value: " << authorized_helper;
      SendErrorAndExit(std::move(response), ErrorCode::INCOMPATIBLE_PROTOCOL);
      return;
    }
  }

  It2MeHost::CreateDeferredConnectContext create_connection_context;
  if (use_signaling_proxy) {
    if (username.empty()) {
      // Allow unauthenticated users for the delegated signal strategy case.
      username = kAnonymousUserName;
    }
    auto signal_strategy = CreateDelegatedSignalStrategy(message);
    if (signal_strategy) {
      create_connection_context = base::BindOnce(
          [](std::unique_ptr<remoting::SignalStrategy> signal_strategy,
             ChromotingHostContext* context) {
            auto connection_context =
                std::make_unique<It2MeHost::DeferredConnectContext>();
            connection_context->register_request =
                std::make_unique<XmppRegisterSupportHostRequest>(
                    kDirectoryBotJidValue);
            connection_context->log_to_server =
                std::make_unique<XmppLogToServer>(
                    ServerLogEntry::IT2ME, signal_strategy.get(),
                    kDirectoryBotJidValue, context->network_task_runner());
            connection_context->signal_strategy = std::move(signal_strategy);
            return connection_context;
          },
          std::move(signal_strategy));
    }
  } else {
    if (!username.empty()) {
      std::string access_token = ExtractAccessToken(message);
      create_connection_context = base::BindOnce(
          [](const std::string& username, const std::string& access_token,
             ChromotingHostContext* host_context) {
            auto connection_context =
                std::make_unique<It2MeHost::DeferredConnectContext>();
            connection_context->use_ftl_signaling = true;
            connection_context->signal_strategy =
                std::make_unique<FtlSignalStrategy>(
                    std::make_unique<PassthroughOAuthTokenGetter>(username,
                                                                  access_token),
                    host_context->url_loader_factory(),
                    std::make_unique<FtlClientUuidDeviceIdProvider>());
            connection_context->register_request =
                std::make_unique<RemotingRegisterSupportHostRequest>(
                    std::make_unique<PassthroughOAuthTokenGetter>(username,
                                                                  access_token),
                    host_context->url_loader_factory());
            connection_context->log_to_server =
                std::make_unique<RemotingLogToServer>(
                    ServerLogEntry::IT2ME,
                    std::make_unique<PassthroughOAuthTokenGetter>(username,
                                                                  access_token),
                    host_context->url_loader_factory());
            connection_context->oauth_token_getter =
                std::make_unique<PassthroughOAuthTokenGetter>(username,
                                                              access_token);
            return connection_context;
          },
          username, access_token);
    } else {
      LOG(ERROR) << "'userName' not found in request.";
    }
  }
  if (!create_connection_context) {
    SendErrorAndExit(std::move(response), ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  protocol::IceConfig ice_config;
  base::Value::Dict* ice_config_dict = message.FindDict(kIceConfig);
  if (ice_config_dict) {
    ice_config = protocol::IceConfig::Parse(*ice_config_dict);
  }

  base::Value::Dict policies = policy_watcher_->GetEffectivePolicies();
  if (policies.empty()) {
    // At this point policies have been read, so if there are none set then
    // it indicates an error. Since this can be fixed by end users it has a
    // dedicated message type rather than the generic "error" so that the
    // right error message can be displayed.
    SendPolicyErrorAndExit();
    return;
  }

  // Create the It2Me host and start connecting. Note that disabling dialogs is
  // only supported on ChromeOS.
  it2me_host_ = factory_->CreateIt2MeHost();
  it2me_host_->set_authorized_helper(authorized_helper);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  it2me_host_->set_enable_dialogs(!suppress_user_dialogs);
  it2me_host_->set_enable_notifications(!suppress_notifications);
  it2me_host_->set_terminate_upon_input(terminate_upon_input);
  it2me_host_->set_enable_curtaining(curtain_local_user_session);
  it2me_host_->set_is_enterprise_session(is_enterprise_admin_user);
#endif
  it2me_host_->Connect(
      host_context_->Copy(), std::move(policies),
      std::make_unique<It2MeConfirmationDialogFactory>(
          is_enterprise_admin_user
              ? It2MeConfirmationDialog::DialogStyle::kEnterprise
              : It2MeConfirmationDialog::DialogStyle::kConsumer),
      weak_ptr_, std::move(create_connection_context), username, ice_config);

  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::ProcessDisconnect(base::Value::Dict message,
                                                 base::Value::Dict response) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(policy_received_);

  if (use_elevated_host_) {
    // Attempt to pass the current message to the elevated process.  This method
    // will spin up the elevated process if it is not already running.  On
    // success, the elevated process will process the message and respond.
    // If the process cannot be started or message passing fails, then return an
    // error to the message sender.
    if (!DelegateToElevatedHost(std::move(message))) {
      LOG(ERROR) << "Failed to send message to elevated host.";
      SendErrorAndExit(std::move(response), ErrorCode::ELEVATION_ERROR);
    }
    return;
  }

  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = nullptr;
  }
  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::ProcessIncomingIq(base::Value::Dict message,
                                                 base::Value::Dict response) {
  if (use_elevated_host_) {
    // Attempt to pass the current message to the elevated process.  This method
    // will spin up the elevated process if it is not already running.  On
    // success, the elevated process will process the message and respond.
    // If the process cannot be started or message passing fails, then return an
    // error to the message sender.
    if (!DelegateToElevatedHost(std::move(message))) {
      LOG(ERROR) << "Failed to send message to elevated host.";
      SendErrorAndExit(std::move(response), ErrorCode::ELEVATION_ERROR);
    }
    return;
  }

  const std::string* iq = message.FindString(kIq);
  if (!iq) {
    LOG(ERROR) << "Invalid incomingIq() data.";
    return;
  }

  if (incoming_message_callback_) {
    incoming_message_callback_.Run(*iq);
  } else {
    LOG(WARNING) << "Dropping message because signaling is not connected. "
                 << "Current It2MeHost state: "
                 << It2MeHostStateToString(state_);
  }
  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::SendOutgoingIq(const std::string& iq) {
  base::Value::Dict message;
  message.Set(kMessageType, kSendOutgoingIqMessage);
  message.Set(kIq, iq);

  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHost::SendErrorAndExit(
    base::Value::Dict response,
    protocol::ErrorCode error_code) const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  response.Set(kMessageType, kErrorMessage);
  response.Set(kErrorMessageCode, ErrorCodeToString(error_code));
  SendMessageToClient(std::move(response));

  // Trigger a host shutdown by sending an empty message.
  client_->CloseChannel(std::string());
}

void It2MeNativeMessagingHost::SendPolicyErrorAndExit() const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  base::Value::Dict message;
  message.Set(kMessageType, kPolicyErrorMessage);
  SendMessageToClient(std::move(message));
  client_->CloseChannel(std::string());
}

void It2MeNativeMessagingHost::OnStateChanged(It2MeHostState state,
                                              protocol::ErrorCode error_code) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  state_ = state;

  base::Value::Dict message;

  message.Set(kMessageType, kHostStateChangedMessage);
  message.Set(kState, It2MeHostStateToString(state));

  switch (state_) {
    case It2MeHostState::kReceivedAccessCode:
      message.Set(kAccessCode, access_code_);
      // base::Value::Dict does not support setting int64_t due to JSON spec's
      // limitation (see comments in values.h). The cast should be safe given
      // the lifetime is relatively short.
      message.Set(kAccessCodeLifetime,
                  static_cast<int>(access_code_lifetime_.InSeconds()));
      break;

    case It2MeHostState::kConnected:
      message.Set(kClient, client_username_);
      break;

    case It2MeHostState::kDisconnected:
      message.Set(kDisconnectReason, ErrorCodeToString(error_code));
      client_username_.clear();
      break;

    case It2MeHostState::kError:
      // kError is an internal-only state, sent to the web-app by a separate
      // "error" message so that errors that occur before the "connect" message
      // is sent can be communicated.
      message.Set(kMessageType, kErrorMessage);
      message.Set(kErrorMessageCode, ErrorCodeToString(error_code));
      break;

    default:
      break;
  }

  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHost::SetPolicyErrorClosureForTesting(
    base::OnceClosure closure) {
  policy_error_closure_for_testing_ = std::move(closure);
}

void It2MeNativeMessagingHost::OnNatPoliciesChanged(
    bool nat_traversal_enabled,
    bool relay_connections_allowed) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  base::Value::Dict message;

  message.Set(kMessageType, kNatPolicyChangedMessage);
  message.Set(kNatPolicyChangedMessageNatEnabled, nat_traversal_enabled);
  message.Set(kNatPolicyChangedMessageRelayEnabled, relay_connections_allowed);
  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHost::OnStoreAccessCode(
    const std::string& access_code,
    base::TimeDelta access_code_lifetime) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  access_code_ = access_code;
  access_code_lifetime_ = access_code_lifetime;
}

void It2MeNativeMessagingHost::OnClientAuthenticated(
    const std::string& client_username) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  client_username_ = client_username;
}

scoped_refptr<base::SingleThreadTaskRunner>
It2MeNativeMessagingHost::task_runner() const {
  return host_context_->ui_task_runner();
}

/* static */

void It2MeNativeMessagingHost::OnPolicyUpdate(base::Value::Dict policies) {
  // If an It2MeHost exists, provide it with the updated policies first.
  // That way it won't appear that the policies have changed if the pending
  // connect callback is run. If done the other way around, there is a race
  // condition which could cause the connection to be canceled before it starts.
  if (it2me_host_) {
    it2me_host_->OnPolicyUpdate(std::move(policies));
  }

  if (!policy_received_) {
    policy_received_ = true;

    if (pending_connect_) {
      std::move(pending_connect_).Run();
    }
  }
}

absl::optional<bool>
It2MeNativeMessagingHost::GetAllowElevatedHostPolicyValue() {
  DCHECK(policy_received_);
#if BUILDFLAG(IS_WIN)
  base::Value::Dict platform_policies = policy_watcher_->GetPlatformPolicies();
  auto* platform_policy_value = platform_policies.FindByDottedPath(
      policy::key::kRemoteAccessHostAllowUiAccessForRemoteAssistance);
  if (platform_policy_value) {
    // Use the platform policy value.
    bool value = platform_policy_value->GetBool();
    LOG(INFO) << "Allow UiAccess for remote support policy value: " << value;
    return value;
  }
#endif  // BUILDFLAG(IS_WIN)

  return absl::nullopt;
}

void It2MeNativeMessagingHost::OnPolicyError() {
  LOG(ERROR) << "Malformed policies detected.";
  policy_received_ = true;

  if (policy_error_closure_for_testing_) {
    std::move(policy_error_closure_for_testing_).Run();
  }

  if (it2me_host_) {
    // If there is already a connection, close it and notify the webapp.
    it2me_host_->Disconnect();
    it2me_host_ = nullptr;
    SendPolicyErrorAndExit();
  } else if (pending_connect_) {
    // If there is no connection, run the pending connection callback if there
    // is one, but otherwise do nothing. The policy error will be sent when a
    // connection is made; doing so beforehand would break assumptions made by
    // the Chrome app.
    std::move(pending_connect_).Run();
  }
}

std::unique_ptr<SignalStrategy>
It2MeNativeMessagingHost::CreateDelegatedSignalStrategy(
    const base::Value::Dict& message) {
  const std::string* local_jid = message.FindString(kLocalJid);
  if (!local_jid) {
    LOG(ERROR) << "'localJid' not found in request.";
    return nullptr;
  }

  auto delegating_signal_strategy = std::make_unique<DelegatingSignalStrategy>(
      SignalingAddress(*local_jid), host_context_->network_task_runner(),
      base::BindRepeating(&It2MeNativeMessagingHost::SendOutgoingIq,
                          weak_factory_.GetWeakPtr()));
  incoming_message_callback_ =
      delegating_signal_strategy->GetIncomingMessageCallback();
  return delegating_signal_strategy;
}

std::string It2MeNativeMessagingHost::ExtractAccessToken(
    const base::Value::Dict& message) {
  const std::string* auth_service_with_token =
      message.FindString(kAuthServiceWithToken);
  if (!auth_service_with_token) {
    LOG(ERROR) << "'authServiceWithToken' not found in request.";
    return {};
  }

  // For backward compatibility the webapp still passes OAuth service as part
  // of the authServiceWithToken field. But auth service part is always
  // expected to be set to oauth2.
  const char kOAuth2ServicePrefix[] = "oauth2:";
  if (!base::StartsWith(*auth_service_with_token, kOAuth2ServicePrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Invalid 'authServiceWithToken': "
               << *auth_service_with_token;
    return {};
  }

  return auth_service_with_token->substr(strlen(kOAuth2ServicePrefix));
}

#if BUILDFLAG(IS_WIN)

bool It2MeNativeMessagingHost::DelegateToElevatedHost(
    base::Value::Dict message) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(use_elevated_host_);

  if (!elevated_host_) {
    base::FilePath binary_path =
        base::CommandLine::ForCurrentProcess()->GetProgram();
    CHECK(binary_path.BaseName() == base::FilePath(kBaseHostBinaryName));

    // The new process runs at an elevated level due to being granted uiAccess.
    // |parent_window_handle| can be used to position dialog windows but is not
    // currently used.
    elevated_host_ = std::make_unique<ElevatedNativeMessagingHost>(
        binary_path.DirName().Append(kElevatedHostBinaryName),
        /*parent_window_handle=*/0,
        /*elevate_process=*/false,
        /*host_timeout=*/base::TimeDelta(), client_);
  }

  if (elevated_host_->EnsureElevatedHostCreated() ==
      PROCESS_LAUNCH_RESULT_SUCCESS) {
    elevated_host_->SendMessage(
        base::Value::ToUniquePtrValue(base::Value(std::move(message))));
    return true;
  }

  return false;
}

#else  // !BUILDFLAG(IS_WIN)

bool It2MeNativeMessagingHost::DelegateToElevatedHost(
    base::Value::Dict message) {
  NOTREACHED();
  return false;
}

#endif  // !BUILDFLAG(IS_WIN)

}  // namespace remoting
