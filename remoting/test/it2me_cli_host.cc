// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/it2me_cli_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/test/test_oauth_token_getter.h"
#include "remoting/test/test_token_storage.h"

namespace remoting {

namespace {

// Communication with CRD Host, messages sent to host:
constexpr char kCRDMessageTypeKey[] = "type";

constexpr char kCRDMessageHello[] = "hello";
constexpr char kCRDMessageConnect[] = "connect";
constexpr char kCRDMessageDisconnect[] = "disconnect";

// Communication with CRD Host, messages received from host:
constexpr char kCRDResponseHello[] = "helloResponse";
constexpr char kCRDResponseConnect[] = "connectResponse";
constexpr char kCRDStateChanged[] = "hostStateChanged";
constexpr char kCRDResponseDisconnect[] = "disconnectResponse";
constexpr char kCRDDebugLog[] = "_debug_log";

// Connect message parameters:
constexpr char kCRDConnectUserName[] = "userName";
constexpr char kCRDConnectAuth[] = "authServiceWithToken";
constexpr char kCRDConnectNoDialogs[] = "noDialogs";

// CRD host states we care about:
constexpr char kCRDStateKey[] = "state";
constexpr char kCRDStateError[] = "ERROR";
constexpr char kCRDStateStarting[] = "STARTING";
constexpr char kCRDStateAccessCodeRequested[] = "REQUESTED_ACCESS_CODE";
constexpr char kCRDStateDomainError[] = "INVALID_DOMAIN_ERROR";
constexpr char kCRDStateAccessCode[] = "RECEIVED_ACCESS_CODE";
constexpr char kCRDStateRemoteDisconnected[] = "DISCONNECTED";
constexpr char kCRDStateRemoteConnected[] = "CONNECTED";

constexpr char kCRDErrorCodeKey[] = "error_code";
constexpr char kCRDAccessCodeKey[] = "accessCode";
constexpr char kCRDAccessCodeLifetimeKey[] = "accessCodeLifetime";

constexpr char kCRDConnectClientKey[] = "client";

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameUsername[] = "username";
constexpr char kSwitchNameStoragePath[] = "storage-path";

std::unique_ptr<It2MeNativeMessagingHost> CreateNativeMessagingHost(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
  auto context = ChromotingHostContext::Create(ui_task_runner);
  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::CreateWithTaskRunner(context->file_task_runner());
  auto factory = std::make_unique<It2MeHostFactory>();
  return std::make_unique<It2MeNativeMessagingHost>(
      /* needs_elevation */ false, std::move(policy_watcher),
      std::move(context), std::move(factory));
}

}  // namespace

// static
bool It2MeCliHost::ShouldPrintHelp() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchNameHelp);
}

// static
void It2MeCliHost::PrintHelp() {
  fprintf(stderr,
          "Usage: %s [--storage-path=<storage-path>] "
          "[--username=<example@gmail.com>]\n ",
          base::CommandLine::ForCurrentProcess()
              ->GetProgram()
              .AsUTF8Unsafe()
              .c_str());
}

It2MeCliHost::It2MeCliHost() {}
It2MeCliHost::~It2MeCliHost() = default;

void It2MeCliHost::Start() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string username = cmd_line->GetSwitchValueASCII(kSwitchNameUsername);
  base::FilePath storage_path =
      cmd_line->GetSwitchValuePath(kSwitchNameStoragePath);

  storage_ = test::TestTokenStorage::OnDisk(username, storage_path);
  token_getter_ = std::make_unique<test::TestOAuthTokenGetter>(storage_.get());

  base::RunLoop initialize_token_getter_loop;
  token_getter_->Initialize(initialize_token_getter_loop.QuitClosure());
  initialize_token_getter_loop.Run();

  base::RunLoop ui_loop;
  ui_task_runner_ = new AutoThreadTaskRunner(
      base::ThreadTaskRunnerHandle::Get(), ui_loop.QuitClosure());

  token_getter_->CallWithToken(base::BindOnce(
      &It2MeCliHost::StartCRDHostAndGetCode, base::Unretained(this)));

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::CreateIfNeeded());
  ui_loop.Run();
}

void It2MeCliHost::PostMessageFromNativeHost(const std::string& message) {
  auto message_value = base::JSONReader::Read(message);
  if (!message_value || !message_value->is_dict()) {
    OnProtocolBroken("Message is not a dictionary");
    return;
  }

  auto* type_value = message_value->FindKeyOfType(kCRDMessageTypeKey,
                                                  base::Value::Type::STRING);
  if (!type_value) {
    OnProtocolBroken("Message without type");
    return;
  }
  std::string type = type_value->GetString();

  if (type == kCRDResponseHello) {
    OnHelloResponse();
  } else if (type == kCRDResponseConnect) {
    // Ok, just ignore.
  } else if (type == kCRDResponseDisconnect) {
    OnDisconnectResponse();
  } else if (type == kCRDStateChanged) {
    // Handle CRD host state changes
    auto* state_value =
        message_value->FindKeyOfType(kCRDStateKey, base::Value::Type::STRING);
    if (!state_value) {
      OnProtocolBroken("No state in message");
      return;
    }
    std::string state = state_value->GetString();

    if (state == kCRDStateAccessCode) {
      OnStateReceivedAccessCode(*message_value);
    } else if (state == kCRDStateRemoteConnected) {
      OnStateRemoteConnected(*message_value);
    } else if (state == kCRDStateRemoteDisconnected) {
      OnStateRemoteDisconnected();
    } else if (state == kCRDStateError || state == kCRDStateDomainError) {
      OnStateError(state, *message_value);
    } else if (state == kCRDStateStarting ||
               state == kCRDStateAccessCodeRequested) {
      // Just ignore these states.
    } else {
      LOG(WARNING) << "Unhandled state: " << state;
    }
  } else if (type == kCRDDebugLog) {
    // The It2Me host already prints the log to stdout/stderr.
  } else {
    LOG(WARNING) << "Unknown message type: " << type;
  }
}

void It2MeCliHost::CloseChannel(const std::string& error_message) {
  LOG(ERROR) << "CRD Host closed channel: " << error_message;
  command_awaiting_crd_access_code_ = false;

  ShutdownHost();
}

void It2MeCliHost::SendMessageToHost(const std::string& type,
                                     base::Value params) {
  std::string message_json;
  params.SetKey(kCRDMessageTypeKey, base::Value(type));
  base::JSONWriter::Write(params, &message_json);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeCliHost::DoSendMessage,
                                weak_factory_.GetWeakPtr(), message_json));
}

void It2MeCliHost::DoSendMessage(const std::string& json) {
  if (!host_)
    return;
  host_->OnMessage(json);
}

void It2MeCliHost::OnProtocolBroken(const std::string& message) {
  LOG(ERROR) << "Error communicating with CRD Host : " << message;
  command_awaiting_crd_access_code_ = false;

  ShutdownHost();
}

void It2MeCliHost::StartCRDHostAndGetCode(OAuthTokenGetter::Status status,
                                          const std::string& user_email,
                                          const std::string& access_token) {
  DCHECK(!host_);

  // Store all parameters for future connect call.
  base::Value connect_params(base::Value::Type::DICTIONARY);

  connect_params.SetKey(kCRDConnectUserName, base::Value(user_email));
  connect_params.SetKey(kCRDConnectAuth, base::Value("oauth2:" + access_token));
  connect_params.SetKey(kCRDConnectNoDialogs, base::Value(true));
  connect_params_ = std::move(connect_params);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  host_ = CreateNativeMessagingHost(ui_task_runner_);
  host_->Start(this);

  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(kCRDMessageHello, std::move(params));
}

void It2MeCliHost::ShutdownHost() {
  if (!host_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeCliHost::DoShutdownHost,
                                weak_factory_.GetWeakPtr()));
}

void It2MeCliHost::DoShutdownHost() {
  host_.reset();
  ui_task_runner_.reset();
}

void It2MeCliHost::OnHelloResponse() {
  // Host is initialized, start connection.
  SendMessageToHost(kCRDMessageConnect, std::move(connect_params_));
}

void It2MeCliHost::OnDisconnectResponse() {
  // Should happen only when remoting session finished and we
  // have requested host to shut down, or when we have got second auth code
  // without receiving connection.
  DCHECK(!command_awaiting_crd_access_code_);
  DCHECK(!remote_connected_);
  ShutdownHost();
}

void It2MeCliHost::OnStateError(const std::string& error_state,
                                const base::Value& message) {
  std::string error_message;
  if (error_state == kCRDStateDomainError) {
    error_message = "CRD Error : Invalid domain";
  } else {
    auto* error_code_value =
        message.FindKeyOfType(kCRDErrorCodeKey, base::Value::Type::STRING);
    if (error_code_value)
      error_message = error_code_value->GetString();
    else
      error_message = "Unknown CRD Error";
  }
  // Notify callback if command is still running.
  if (command_awaiting_crd_access_code_) {
    command_awaiting_crd_access_code_ = false;
    LOG(ERROR) << "CRD Error state " + error_state;
  }
  // Shut down host, if any
  ShutdownHost();
}

void It2MeCliHost::OnStateRemoteConnected(const base::Value& message) {
  remote_connected_ = true;
  auto* client_value =
      message.FindKeyOfType(kCRDConnectClientKey, base::Value::Type::STRING);
  if (client_value) {
    HOST_LOG << "Remote connection by " << client_value->GetString();
  }
}

void It2MeCliHost::OnStateRemoteDisconnected() {
  // There could be a connection attempt that was not successful, we will
  // receive "disconnected" message without actually receiving "connected".
  if (!remote_connected_)
    return;
  remote_connected_ = false;
  // Remote has disconnected, time to send "disconnect" that would result
  // in shutting down the host.
  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(kCRDMessageDisconnect, std::move(params));
}

void It2MeCliHost::OnStateReceivedAccessCode(const base::Value& message) {
  if (!command_awaiting_crd_access_code_) {
    if (!remote_connected_) {
      // We have already sent the access code back to the server which initiated
      // this CRD session through a remote command, and we can not send a new
      // access code. Assuming that the old access code is no longer valid, we
      // can only terminate the current CRD session.
      base::Value params(base::Value::Type::DICTIONARY);
      SendMessageToHost(kCRDMessageDisconnect, std::move(params));
    }
    return;
  }

  auto* code_value =
      message.FindKeyOfType(kCRDAccessCodeKey, base::Value::Type::STRING);
  auto* code_lifetime_value = message.FindKeyOfType(kCRDAccessCodeLifetimeKey,
                                                    base::Value::Type::INTEGER);
  if (!code_value || !code_lifetime_value) {
    OnProtocolBroken("Can not obtain access code");
    return;
  }
  command_awaiting_crd_access_code_ = false;

  // Prints the access code.
  base::TimeDelta expires_in =
      base::TimeDelta::FromSeconds(code_lifetime_value->GetInt());
  HOST_LOG << "It2Me access code is generated: " << code_value->GetString();
  HOST_LOG << "Expires at: " << (base::Time::Now() + expires_in);
}

}  // namespace remoting
