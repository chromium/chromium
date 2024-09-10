// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/it2me_cli_host.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/test/test_oauth_token_getter.h"
#include "remoting/test/test_token_storage.h"

namespace remoting {

namespace {

constexpr char kCRDDebugLog[] = "_debug_log";

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameUsername[] = "username";
constexpr char kSwitchNameStoragePath[] = "storage-path";

std::unique_ptr<It2MeNativeMessagingHost> CreateNativeMessagingHost(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
  auto context = ChromotingHostContext::Create(ui_task_runner);
  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::CreateWithTaskRunner(context->file_task_runner(),
                                          context->management_service());
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
      base::SingleThreadTaskRunner::GetCurrentDefault(), ui_loop.QuitClosure());

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

  base::Value::Dict& message_dict = message_value->GetDict();
  std::string* type = message_dict.FindString(kMessageType);
  if (!type) {
    OnProtocolBroken("Message without type");
    return;
  }

  if (*type == kHelloResponse) {
    OnHelloResponse();
  } else if (*type == kConnectResponse) {
    // Ok, just ignore.
  } else if (*type == kDisconnectResponse) {
    OnDisconnectResponse();
  } else if (*type == kHostStateChangedMessage) {
    // Handle CRD host state changes
    std::string* state = message_dict.FindString(kState);
    if (!state) {
      OnProtocolBroken("No state in message");
      return;
    }

    if (*state == kHostStateReceivedAccessCode) {
      OnStateReceivedAccessCode(message_dict);
    } else if (*state == kHostStateConnected) {
      OnStateRemoteConnected(message_dict);
    } else if (*state == kHostStateDisconnected) {
      OnStateRemoteDisconnected();
    } else if (*state == kHostStateError || *state == kHostStateDomainError) {
      OnStateError(*state, message_dict);
    } else if (*state == kHostStateStarting ||
               *state == kHostStateRequestedAccessCode) {
      // Just ignore these states.
    } else {
      LOG(WARNING) << "Unhandled state: " << *state;
    }
  } else if (*type == kCRDDebugLog) {
    // The It2Me host already prints the log to stdout/stderr.
  } else {
    LOG(WARNING) << "Unknown message type: " << *type;
  }
}

void It2MeCliHost::CloseChannel(const std::string& error_message) {
  LOG(ERROR) << "CRD Host closed channel: " << error_message;
  command_awaiting_crd_access_code_ = false;

  ShutdownHost();
}

void It2MeCliHost::SendMessageToHost(const std::string& type,
                                     base::Value::Dict params) {
  std::string message_json;
  params.Set(kMessageType, type);
  base::JSONWriter::Write(params, &message_json);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeCliHost::DoSendMessage,
                                weak_factory_.GetWeakPtr(), message_json));
}

void It2MeCliHost::DoSendMessage(const std::string& json) {
  if (!host_) {
    return;
  }
  host_->OnMessage(json);
}

void It2MeCliHost::OnProtocolBroken(const std::string& message) {
  LOG(ERROR) << "Error communicating with CRD Host : " << message;
  command_awaiting_crd_access_code_ = false;

  ShutdownHost();
}

void It2MeCliHost::StartCRDHostAndGetCode(OAuthTokenGetter::Status status,
                                          const std::string& user_email,
                                          const std::string& access_token,
                                          const std::string& scopes) {
  DCHECK(!host_);

  // Store all parameters for future connect call.
  connect_params_ = base::Value::Dict()
                        .Set(kUserName, user_email)
                        .Set(kAuthServiceWithToken, "oauth2:" + access_token)
                        .Set(kAccessToken, access_token)
                        .Set(kSuppressUserDialogs, true)
                        .Set(kSuppressNotifications, true);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  host_ = CreateNativeMessagingHost(ui_task_runner_);
  host_->Start(this);

  SendMessageToHost(kHelloMessage, base::Value::Dict());
}

void It2MeCliHost::ShutdownHost() {
  if (!host_) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeCliHost::DoShutdownHost,
                                weak_factory_.GetWeakPtr()));
}

void It2MeCliHost::DoShutdownHost() {
  host_.reset();
  ui_task_runner_.reset();
}

void It2MeCliHost::OnHelloResponse() {
  // Host is initialized, start connection.
  SendMessageToHost(kConnectMessage, std::move(connect_params_));
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
                                const base::Value::Dict& message) {
  std::string error_message;
  if (error_state == kHostStateDomainError) {
    error_message = "CRD Error : Invalid domain";
  } else {
    const std::string* error_code = message.FindString(kErrorMessageCode);
    if (error_code) {
      error_message = *error_code;
    } else {
      error_message = "Unknown CRD Error";
    }
  }
  // Notify callback if command is still running.
  if (command_awaiting_crd_access_code_) {
    command_awaiting_crd_access_code_ = false;
    LOG(ERROR) << "CRD Error state " + error_state;
  }
  // Shut down host, if any
  ShutdownHost();
}

void It2MeCliHost::OnStateRemoteConnected(const base::Value::Dict& message) {
  remote_connected_ = true;
  const std::string* client = message.FindString(kClient);
  if (client) {
    HOST_LOG << "Remote connection by " << *client;
  }
}

void It2MeCliHost::OnStateRemoteDisconnected() {
  // There could be a connection attempt that was not successful, we will
  // receive "disconnected" message without actually receiving "connected".
  if (!remote_connected_) {
    return;
  }
  remote_connected_ = false;
  // Remote has disconnected, time to send "disconnect" that would result
  // in shutting down the host.
  SendMessageToHost(kDisconnectMessage, base::Value::Dict());
}

void It2MeCliHost::OnStateReceivedAccessCode(const base::Value::Dict& message) {
  if (!command_awaiting_crd_access_code_) {
    if (!remote_connected_) {
      // We have already sent the access code back to the server which initiated
      // this CRD session through a remote command, and we can not send a new
      // access code. Assuming that the old access code is no longer valid, we
      // can only terminate the current CRD session.
      SendMessageToHost(kDisconnectMessage, base::Value::Dict());
    }
    return;
  }

  const std::string* code = message.FindString(kAccessCode);
  const std::optional<int> code_lifetime = message.FindInt(kAccessCodeLifetime);
  if (!code || !code_lifetime) {
    OnProtocolBroken("Can not obtain access code");
    return;
  }
  command_awaiting_crd_access_code_ = false;

  // Prints the access code.
  base::TimeDelta expires_in = base::Seconds(*code_lifetime);
  HOST_LOG << "It2Me access code is generated: " << *code;
  HOST_LOG << "Expires at: " << (base::Time::Now() + expires_in);
}

}  // namespace remoting
