// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "net/base/url_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/name_value_map.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
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

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/files/file_path.h"

#include "remoting/host/win/elevated_native_messaging_host.h"
#endif  // defined(OS_WIN)

namespace remoting {

using protocol::ErrorCode;

namespace {

const NameMapElement<It2MeHostState> kIt2MeHostStates[] = {
    {kDisconnected, "DISCONNECTED"},
    {kStarting, "STARTING"},
    {kRequestedAccessCode, "REQUESTED_ACCESS_CODE"},
    {kReceivedAccessCode, "RECEIVED_ACCESS_CODE"},
    {kConnecting, "CONNECTING"},
    {kConnected, "CONNECTED"},
    {kError, "ERROR"},
    {kInvalidDomainError, "INVALID_DOMAIN_ERROR"},
};

#if defined(OS_WIN)
const base::FilePath::CharType kBaseHostBinaryName[] =
    FILE_PATH_LITERAL("remote_assistance_host.exe");
const base::FilePath::CharType kElevatedHostBinaryName[] =
    FILE_PATH_LITERAL("remote_assistance_host_uiaccess.exe");
#endif  // defined(OS_WIN)

constexpr char kAnonymousUserName[] = "anonymous_user";
const char kRemotingBotJid[] = "remoting@bot.talk.google.com";

// Helper functions to run |callback| asynchronously on the correct thread
// using |task_runner|.
void PolicyUpdateCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    remoting::PolicyWatcher::PolicyUpdatedCallback callback,
    std::unique_ptr<base::DictionaryValue> policies) {
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

}  // namespace

It2MeNativeMessagingHost::It2MeNativeMessagingHost(
    bool needs_elevation,
    std::unique_ptr<PolicyWatcher> policy_watcher,
    std::unique_ptr<ChromotingHostContext> context,
    std::unique_ptr<It2MeHostFactory> factory)
    : needs_elevation_(needs_elevation),
      host_context_(std::move(context)),
      factory_(std::move(factory)),
      policy_watcher_(std::move(policy_watcher)) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // The policy watcher runs on the |file_task_runner| but we want to run the
  // callbacks on |task_runner| so we use a shim to post them to it.
  PolicyWatcher::PolicyUpdatedCallback update_callback =
      base::Bind(&It2MeNativeMessagingHost::OnPolicyUpdate, weak_ptr_);
  PolicyWatcher::PolicyErrorCallback error_callback =
      base::Bind(&It2MeNativeMessagingHost::OnPolicyError, weak_ptr_);
  policy_watcher_->StartWatching(
      base::Bind(&PolicyUpdateCallback, task_runner(), update_callback),
      base::Bind(&PolicyErrorCallback, task_runner(), error_callback));
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

  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(message);
  if (!message_value->is_dict()) {
    LOG(ERROR) << "Received a message that's not a dictionary.";
    client_->CloseChannel(std::string());
    return;
  }

  std::unique_ptr<base::DictionaryValue> message_dict(
      static_cast<base::DictionaryValue*>(message_value.release()));

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message_dict->Get("id", &id))
    response->SetKey("id", id->Clone());

  std::string type;
  if (!message_dict->GetString("type", &type)) {
    LOG(ERROR) << "'type' not found in request.";
    SendErrorAndExit(std::move(response), ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  response->SetString("type", type + "Response");

  if (type == "hello") {
    ProcessHello(std::move(message_dict), std::move(response));
  } else if (type == "connect") {
    ProcessConnect(std::move(message_dict), std::move(response));
  } else if (type == "disconnect") {
    ProcessDisconnect(std::move(message_dict), std::move(response));
  } else if (type == "incomingIq") {
    ProcessIncomingIq(std::move(message_dict), std::move(response));
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
    SendErrorAndExit(std::move(response), ErrorCode::INCOMPATIBLE_PROTOCOL);
  }
}

void It2MeNativeMessagingHost::Start(Client* client) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  client_ = client;
#if !defined(OS_CHROMEOS)
  log_message_handler_.reset(
      new LogMessageHandler(
          base::Bind(&It2MeNativeMessagingHost::SendMessageToClient,
                     base::Unretained(this))));
#endif  // !defined(OS_CHROMEOS)
}

void It2MeNativeMessagingHost::SendMessageToClient(
    std::unique_ptr<base::Value> message) const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  std::string message_json;
  base::JSONWriter::Write(*message, &message_json);
  client_->PostMessageFromNativeHost(message_json);
}

void It2MeNativeMessagingHost::ProcessHello(
    std::unique_ptr<base::DictionaryValue> message,
    std::unique_ptr<base::DictionaryValue> response) const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // No need to forward to the elevated process since no internal state is set.

  response->SetString("version", STRINGIZE(VERSION));

  // This list will be populated when new features are added.
  response->Set("supportedFeatures", std::make_unique<base::ListValue>());

  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::ProcessConnect(
    std::unique_ptr<base::DictionaryValue> message,
    std::unique_ptr<base::DictionaryValue> response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (!policy_received_) {
    DCHECK(!pending_connect_);
    pending_connect_ =
        base::Bind(&It2MeNativeMessagingHost::ProcessConnect, weak_ptr_,
                   base::Passed(&message), base::Passed(&response));
    return;
  }

  if (needs_elevation_) {
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

  bool use_signaling_proxy = false;
  message->GetBoolean("useSignalingProxy", &use_signaling_proxy);

  std::string username;
  message->GetString("userName", &username);

  bool no_dialogs = false;
  message->GetBoolean("noDialogs", &no_dialogs);

  bool terminate_upon_input = false;
  message->GetBoolean("terminateUponInput", &terminate_upon_input);

  std::unique_ptr<SignalStrategy> signal_strategy;
  std::unique_ptr<RegisterSupportHostRequest> register_host_request;
  std::unique_ptr<LogToServer> log_to_server;
  if (use_signaling_proxy) {
    if (username.empty()) {
      // Allow unauthenticated users for the delegated signal strategy case.
      username = kAnonymousUserName;
    }
    signal_strategy = CreateDelegatedSignalStrategy(message.get());
    register_host_request =
        std::make_unique<XmppRegisterSupportHostRequest>(kRemotingBotJid);
    log_to_server = std::make_unique<XmppLogToServer>(
        ServerLogEntry::IT2ME, signal_strategy.get(), kRemotingBotJid,
        host_context_->network_task_runner());
  } else {
    std::string access_token = ExtractAccessToken(message.get());
    signal_strategy = CreateFtlSignalStrategy(username, access_token);
    register_host_request =
        std::make_unique<RemotingRegisterSupportHostRequest>(
            std::make_unique<PassthroughOAuthTokenGetter>(username,
                                                          access_token));
    log_to_server = std::make_unique<RemotingLogToServer>(
        ServerLogEntry::IT2ME,
        std::make_unique<PassthroughOAuthTokenGetter>(username, access_token));
  }
  if (!signal_strategy) {
    SendErrorAndExit(std::move(response), ErrorCode::INCOMPATIBLE_PROTOCOL);
    return;
  }

  base::DictionaryValue* ice_config_dict;
  protocol::IceConfig ice_config;
  if (message->GetDictionary("iceConfig", &ice_config_dict)) {
    ice_config = protocol::IceConfig::Parse(*ice_config_dict);
  }

  std::unique_ptr<base::DictionaryValue> policies =
      policy_watcher_->GetCurrentPolicies();
  if (policies->size() == 0) {
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
#if defined(OS_CHROMEOS) || !defined(NDEBUG)
  it2me_host_->set_enable_dialogs(!no_dialogs);
  it2me_host_->set_terminate_upon_input(terminate_upon_input);
#endif
  it2me_host_->Connect(host_context_->Copy(), std::move(policies),
                       std::make_unique<It2MeConfirmationDialogFactory>(),
                       std::move(register_host_request),
                       std::move(log_to_server), weak_ptr_,
                       std::move(signal_strategy), username, ice_config);

  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::ProcessDisconnect(
    std::unique_ptr<base::DictionaryValue> message,
    std::unique_ptr<base::DictionaryValue> response) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(policy_received_);

  if (needs_elevation_) {
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

void It2MeNativeMessagingHost::ProcessIncomingIq(
    std::unique_ptr<base::DictionaryValue> message,
    std::unique_ptr<base::DictionaryValue> response) {
  if (needs_elevation_) {
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

  std::string iq;
  if (!message->GetString("iq", &iq)) {
    LOG(ERROR) << "Invalid incomingIq() data.";
    return;
  }

  incoming_message_callback_.Run(iq);
  SendMessageToClient(std::move(response));
}

void It2MeNativeMessagingHost::SendOutgoingIq(const std::string& iq) {
  std::unique_ptr<base::DictionaryValue> message(new base::DictionaryValue());
  message->SetString("iq", iq);
  message->SetString("type", "sendOutgoingIq");
  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHost::SendErrorAndExit(
    std::unique_ptr<base::DictionaryValue> response,
    protocol::ErrorCode error_code) const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  response->SetString("type", "error");
  response->SetString("error_code", ErrorCodeToString(error_code));
  // TODO(kelvinp): Remove this after M61 Webapp is pushed to 100%.
  response->SetString("description", ErrorCodeToString(error_code));
  SendMessageToClient(std::move(response));

  // Trigger a host shutdown by sending an empty message.
  client_->CloseChannel(std::string());
}

void It2MeNativeMessagingHost::SendPolicyErrorAndExit() const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  auto message = std::make_unique<base::DictionaryValue>();
  message->SetString("type", "policyError");
  SendMessageToClient(std::move(message));
  client_->CloseChannel(std::string());
}

void It2MeNativeMessagingHost::OnStateChanged(It2MeHostState state,
                                              protocol::ErrorCode error_code) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  state_ = state;

  std::unique_ptr<base::DictionaryValue> message(new base::DictionaryValue());

  message->SetString("type", "hostStateChanged");
  message->SetString("state", HostStateToString(state));

  switch (state_) {
    case kReceivedAccessCode:
      message->SetString("accessCode", access_code_);
      message->SetInteger("accessCodeLifetime",
                          access_code_lifetime_.InSeconds());
      break;

    case kConnected:
      message->SetString("client", client_username_);
      break;

    case kDisconnected:
      client_username_.clear();
      break;

    case kError:
      // kError is an internal-only state, sent to the web-app by a separate
      // "error" message so that errors that occur before the "connect" message
      // is sent can be communicated.
      message->SetString("type", "error");
      message->SetString("error_code", ErrorCodeToString(error_code));
      // TODO(kelvinp): Remove this after M61 Webapp is pushed to 100%.
      message->SetString("description", ErrorCodeToString(error_code));
      break;

    default:
      break;
  }

  SendMessageToClient(std::move(message));
}

void It2MeNativeMessagingHost::SetPolicyErrorClosureForTesting(
    const base::Closure& closure) {
  policy_error_closure_for_testing_ = closure;
}

void It2MeNativeMessagingHost::OnNatPolicyChanged(bool nat_traversal_enabled) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  std::unique_ptr<base::DictionaryValue> message(new base::DictionaryValue());

  message->SetString("type", "natPolicyChanged");
  message->SetBoolean("natTraversalEnabled", nat_traversal_enabled);
  SendMessageToClient(std::move(message));
}

// Stores the Access Code for the web-app to query.
void It2MeNativeMessagingHost::OnStoreAccessCode(
    const std::string& access_code,
    base::TimeDelta access_code_lifetime) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  access_code_ = access_code;
  access_code_lifetime_ = access_code_lifetime;
}

// Stores the client user's name for the web-app to query.
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
std::string It2MeNativeMessagingHost::HostStateToString(
    It2MeHostState host_state) {
  return ValueToName(kIt2MeHostStates, host_state);
}

void It2MeNativeMessagingHost::OnPolicyUpdate(
    std::unique_ptr<base::DictionaryValue> policies) {
  // Don't dynamically change the elevation status since we don't have a good
  // way to communicate changes to the user.
  if (!policy_received_) {
    bool allow_elevated_host = false;
    if (!policies->GetBoolean(
            policy::key::kRemoteAccessHostAllowUiAccessForRemoteAssistance,
            &allow_elevated_host)) {
      LOG(WARNING) << "Failed to retrieve elevated host policy value.";
    }
#if defined(OS_WIN)
    LOG(INFO) << "Allow UiAccess for Remote Assistance: "
              << allow_elevated_host;
#endif  // defined(OS_WIN)

    policy_received_ = true;

    // If |allow_elevated_host| is false, then we will fall back to using a host
    // running in the current context regardless of the elevation request.  This
    // may not be ideal, but is still functional.
    needs_elevation_ = needs_elevation_ && allow_elevated_host;
    if (pending_connect_) {
      std::move(pending_connect_).Run();
    }
  }

  if (it2me_host_) {
    it2me_host_->OnPolicyUpdate(std::move(policies));
  }
}

void It2MeNativeMessagingHost::OnPolicyError() {
  LOG(ERROR) << "Malformed policies detected.";
  policy_received_ = true;

  if (policy_error_closure_for_testing_) {
    policy_error_closure_for_testing_.Run();
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
    const base::DictionaryValue* message) {
  std::string local_jid;
  if (!message->GetString("localJid", &local_jid)) {
    LOG(ERROR) << "'localJid' not found in request.";
    return nullptr;
  }

  auto delegating_signal_strategy = std::make_unique<DelegatingSignalStrategy>(
      SignalingAddress(local_jid), host_context_->network_task_runner(),
      base::BindRepeating(&It2MeNativeMessagingHost::SendOutgoingIq,
                          weak_factory_.GetWeakPtr()));
  incoming_message_callback_ =
      delegating_signal_strategy->GetIncomingMessageCallback();
  return delegating_signal_strategy;
}

std::unique_ptr<SignalStrategy>
It2MeNativeMessagingHost::CreateFtlSignalStrategy(
    const std::string& username,
    const std::string& access_token) {
  if (username.empty()) {
    LOG(ERROR) << "'userName' not found in request.";
    return nullptr;
  }

  return std::make_unique<FtlSignalStrategy>(
      std::make_unique<PassthroughOAuthTokenGetter>(username, access_token),
      std::make_unique<FtlClientUuidDeviceIdProvider>());
}

std::string It2MeNativeMessagingHost::ExtractAccessToken(
    const base::DictionaryValue* message) {
  std::string auth_service_with_token;
  if (!message->GetString("authServiceWithToken", &auth_service_with_token)) {
    LOG(ERROR) << "'authServiceWithToken' not found in request.";
    return {};
  }

  // For backward compatibility the webapp still passes OAuth service as part
  // of the authServiceWithToken field. But auth service part is always
  // expected to be set to oauth2.
  const char kOAuth2ServicePrefix[] = "oauth2:";
  if (!base::StartsWith(auth_service_with_token, kOAuth2ServicePrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Invalid 'authServiceWithToken': " << auth_service_with_token;
    return {};
  }

  return auth_service_with_token.substr(strlen(kOAuth2ServicePrefix));
}

#if defined(OS_WIN)

bool It2MeNativeMessagingHost::DelegateToElevatedHost(
    std::unique_ptr<base::DictionaryValue> message) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(needs_elevation_);

  if (!elevated_host_) {
    base::FilePath binary_path =
        base::CommandLine::ForCurrentProcess()->GetProgram();
    CHECK(binary_path.BaseName() == base::FilePath(kBaseHostBinaryName));

    // The new process runs at an elevated level due to being granted uiAccess.
    // |parent_window_handle| can be used to position dialog windows but is not
    // currently used.
    elevated_host_.reset(new ElevatedNativeMessagingHost(
        binary_path.DirName().Append(kElevatedHostBinaryName),
        /*parent_window_handle=*/0,
        /*elevate_process=*/false,
        /*host_timeout=*/base::TimeDelta(), client_));
  }

  if (elevated_host_->EnsureElevatedHostCreated() ==
      PROCESS_LAUNCH_RESULT_SUCCESS) {
    elevated_host_->SendMessage(std::move(message));
    return true;
  }

  return false;
}

#else  // !defined(OS_WIN)

bool It2MeNativeMessagingHost::DelegateToElevatedHost(
    std::unique_ptr<base::DictionaryValue> message) {
  NOTREACHED();
  return false;
}

#endif  // !defined(OS_WIN)

}  // namespace remoting
