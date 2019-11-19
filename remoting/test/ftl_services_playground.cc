// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/ftl_services_playground.h"

#include <inttypes.h>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/proto/ftl/v1/ftl_services.grpc.pb.h"
#include "remoting/signaling/ftl_grpc_context.h"
#include "remoting/test/cli_util.h"
#include "remoting/test/test_device_id_provider.h"
#include "remoting/test/test_oauth_token_getter.h"
#include "remoting/test/test_token_storage.h"

namespace {

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameUsername[] = "username";
constexpr char kSwitchNameStoragePath[] = "storage-path";
constexpr char kSwitchNameNoAutoSignin[] = "no-auto-signin";

bool NeedsManualSignin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSwitchNameNoAutoSignin);
}

}  // namespace

namespace remoting {

FtlServicesPlayground::FtlServicesPlayground() {}

FtlServicesPlayground::~FtlServicesPlayground() = default;

bool FtlServicesPlayground::ShouldPrintHelp() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchNameHelp);
}

void FtlServicesPlayground::PrintHelp() {
  printf(
      "Usage: %s [--no-auto-signin] [--auth-code=<auth-code>] "
      "[--storage-path=<storage-path>] [--username=<example@gmail.com>]\n",
      base::CommandLine::ForCurrentProcess()
          ->GetProgram()
          .MaybeAsASCII()
          .c_str());
}

void FtlServicesPlayground::StartAndAuthenticate() {
  DCHECK(!storage_);
  DCHECK(!token_getter_);
  DCHECK(!executor_);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string username = cmd_line->GetSwitchValueASCII(kSwitchNameUsername);
  base::FilePath storage_path =
      cmd_line->GetSwitchValuePath(kSwitchNameStoragePath);
  storage_ = test::TestTokenStorage::OnDisk(username, storage_path);

  token_getter_ = std::make_unique<test::TestOAuthTokenGetter>(storage_.get());

  base::RunLoop run_loop;
  token_getter_->Initialize(
      base::BindOnce(&FtlServicesPlayground::ResetServices,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();

  StartLoop();
}

void FtlServicesPlayground::StartLoop() {
  std::vector<test::CommandOption> options{
      {"GetIceServer", base::BindRepeating(&FtlServicesPlayground::GetIceServer,
                                           weak_factory_.GetWeakPtr())},
      {"PullMessages", base::BindRepeating(&FtlServicesPlayground::PullMessages,
                                           weak_factory_.GetWeakPtr())},
      {"ReceiveMessages",
       base::BindRepeating(&FtlServicesPlayground::StartReceivingMessages,
                           weak_factory_.GetWeakPtr())},
      {"SendMessage", base::BindRepeating(&FtlServicesPlayground::SendMessage,
                                          weak_factory_.GetWeakPtr())}};

  if (NeedsManualSignin()) {
    options.insert(
        options.begin(),
        {"SignInGaia", base::BindRepeating(&FtlServicesPlayground::SignInGaia,
                                           weak_factory_.GetWeakPtr())});
  }

  test::RunCommandOptionsLoop(options);
}

void FtlServicesPlayground::ResetServices(base::OnceClosure on_done) {
  executor_ = std::make_unique<GrpcAuthenticatedExecutor>(token_getter_.get());
  peer_to_peer_stub_ = PeerToPeer::NewStub(FtlGrpcContext::CreateChannel());

  registration_manager_ = std::make_unique<FtlRegistrationManager>(
      token_getter_.get(),
      std::make_unique<test::TestDeviceIdProvider>(storage_.get()));

  message_subscription_.reset();
  messaging_client_ = std::make_unique<FtlMessagingClient>(
      token_getter_.get(), registration_manager_.get());
  message_subscription_ = messaging_client_->RegisterMessageCallback(
      base::BindRepeating(&FtlServicesPlayground::OnMessageReceived,
                          weak_factory_.GetWeakPtr()));

  if (NeedsManualSignin()) {
    std::move(on_done).Run();
  } else {
    SignInGaia(std::move(on_done));
  }
}

void FtlServicesPlayground::GetIceServer(base::OnceClosure on_done) {
  DCHECK(peer_to_peer_stub_);
  VLOG(0) << "Running GetIceServer...";
  ftl::GetICEServerRequest request;
  *request.mutable_header() = FtlGrpcContext::CreateRequestHeader();
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&PeerToPeer::Stub::AsyncGetICEServer,
                     base::Unretained(peer_to_peer_stub_.get())),
      request,
      base::BindOnce(&FtlServicesPlayground::OnGetIceServerResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
  FtlGrpcContext::FillClientContext(grpc_request->context());
  executor_->ExecuteRpc(std::move(grpc_request));
}

void FtlServicesPlayground::OnGetIceServerResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
    const ftl::GetICEServerResponse& response) {
  if (!status.ok()) {
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }

  printf("Ice transport policy: %s\n",
         response.ice_config().ice_transport_policy().c_str());
  for (const ftl::ICEServerList& server : response.ice_config().ice_servers()) {
    printf(
        "ICE server:\n"
        "  hostname=%s\n"
        "  username=%s\n"
        "  credential=%s\n"
        "  max_rate_kbps=%" PRId64 "\n",
        server.hostname().c_str(), server.username().c_str(),
        server.credential().c_str(), server.max_rate_kbps());
    for (const std::string& url : server.urls()) {
      printf("  url=%s\n", url.c_str());
    }
  }
  std::move(on_done).Run();
}

void FtlServicesPlayground::SignInGaia(base::OnceClosure on_done) {
  DCHECK(registration_manager_);
  VLOG(0) << "Running SignInGaia...";
  registration_manager_->SignInGaia(
      base::BindOnce(&FtlServicesPlayground::OnSignInGaiaResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlServicesPlayground::OnSignInGaiaResponse(base::OnceClosure on_done,
                                                 const grpc::Status& status) {
  if (!status.ok()) {
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }

  std::string registration_id_base64;
  base::Base64Encode(registration_manager_->GetRegistrationId(),
                     &registration_id_base64);
  printf("Service signed in. registration_id(base64)=%s\n",
         registration_id_base64.c_str());
  std::move(on_done).Run();
}

void FtlServicesPlayground::PullMessages(base::OnceClosure on_done) {
  DCHECK(messaging_client_);
  VLOG(0) << "Running PullMessages...";

  messaging_client_->PullMessages(
      base::BindOnce(&FtlServicesPlayground::OnPullMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlServicesPlayground::OnPullMessagesResponse(base::OnceClosure on_done,
                                                   const grpc::Status& status) {
  if (!status.ok()) {
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }
  std::move(on_done).Run();
}

void FtlServicesPlayground::SendMessage(base::OnceClosure on_done) {
  DCHECK(messaging_client_);
  VLOG(0) << "Running SendMessage...";

  printf("Receiver ID: ");
  std::string receiver_id = test::ReadString();

  printf("Receiver registration ID (base64, optional): ");
  std::string registration_id_base64 = test::ReadString();

  std::string registration_id;
  bool success = base::Base64Decode(registration_id_base64, &registration_id);
  if (!success) {
    fprintf(stderr, "Your input can't be base64 decoded.\n");
    std::move(on_done).Run();
    return;
  }
  DoSendMessage(receiver_id, registration_id, std::move(on_done), true);
}

void FtlServicesPlayground::DoSendMessage(const std::string& receiver_id,
                                          const std::string& registration_id,
                                          base::OnceClosure on_done,
                                          bool should_keep_running) {
  if (!should_keep_running) {
    std::move(on_done).Run();
    return;
  }

  printf("Message (enter nothing to quit): ");
  std::string message = test::ReadString();

  if (message.empty()) {
    std::move(on_done).Run();
    return;
  }

  auto on_continue = base::BindOnce(&FtlServicesPlayground::DoSendMessage,
                                    weak_factory_.GetWeakPtr(), receiver_id,
                                    registration_id, std::move(on_done));

  ftl::ChromotingMessage crd_message;
  crd_message.mutable_xmpp()->set_stanza(message);
  messaging_client_->SendMessage(
      receiver_id, registration_id, crd_message,
      base::BindOnce(&FtlServicesPlayground::OnSendMessageResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_continue)));
}

void FtlServicesPlayground::OnSendMessageResponse(
    base::OnceCallback<void(bool)> on_continue,
    const grpc::Status& status) {
  if (!status.ok()) {
    HandleGrpcStatusError(base::BindOnce(std::move(on_continue), false),
                          status);
    return;
  }

  printf("Message successfully sent.\n");
  std::move(on_continue).Run(true);
}

void FtlServicesPlayground::StartReceivingMessages(base::OnceClosure on_done) {
  VLOG(0) << "Running StartReceivingMessages...";
  receive_messages_done_callback_ = std::move(on_done);
  messaging_client_->StartReceivingMessages(
      base::BindOnce(&FtlServicesPlayground::OnReceiveMessagesStreamReady,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&FtlServicesPlayground::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void FtlServicesPlayground::StopReceivingMessages(base::OnceClosure on_done) {
  messaging_client_->StopReceivingMessages();
  std::move(on_done).Run();
}

void FtlServicesPlayground::OnMessageReceived(
    const ftl::Id& sender_id,
    const std::string& sender_registration_id,
    const ftl::ChromotingMessage& message) {
  std::string message_text = message.xmpp().stanza();
  printf(
      "Received message:\n"
      "  Sender ID=%s\n"
      "  Sender Registration ID=%s\n"
      "  Message=%s\n",
      sender_id.id().c_str(), sender_registration_id.c_str(),
      message_text.c_str());
}

void FtlServicesPlayground::OnReceiveMessagesStreamReady() {
  printf("Started receiving messages. Press enter to stop streaming...\n");
  test::WaitForEnterKey(base::BindOnce(
      &FtlServicesPlayground::StopReceivingMessages, weak_factory_.GetWeakPtr(),
      std::move(receive_messages_done_callback_)));
}

void FtlServicesPlayground::OnReceiveMessagesStreamClosed(
    const grpc::Status& status) {
  base::OnceClosure callback = std::move(receive_messages_done_callback_);
  bool is_callback_null = callback.is_null();
  if (is_callback_null) {
    callback = base::DoNothing::Once();
  }
  if (status.error_code() == grpc::StatusCode::CANCELLED) {
    printf("ReceiveMessages stream canceled by client.\n");
    std::move(callback).Run();
    return;
  }

  if (!status.ok()) {
    HandleGrpcStatusError(std::move(callback), status);
  } else {
    printf("Stream closed by server.\n");
    std::move(callback).Run();
  }

  if (is_callback_null) {
    // Stream had been started and callback has been passed to wait for the
    // enter key.
    printf("Please press enter to continue...\n");
  }
}

void FtlServicesPlayground::HandleGrpcStatusError(base::OnceClosure on_done,
                                                  const grpc::Status& status) {
  DCHECK(!status.ok());
  if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
    if (NeedsManualSignin()) {
      printf(
          "Request is unauthenticated. You should run SignInGaia first if "
          "you haven't done so, otherwise your OAuth token might be expired. \n"
          "Request for new OAuth token? [y/N]: ");
      if (!test::ReadYNBool()) {
        std::move(on_done).Run();
        return;
      }
    }
    VLOG(0) << "Grpc request failed to authenticate. "
            << "Trying to reauthenticate...";
    token_getter_->ResetWithAuthenticationFlow(
        base::BindOnce(&FtlServicesPlayground::ResetServices,
                       weak_factory_.GetWeakPtr(), std::move(on_done)));
    return;
  }

  fprintf(stderr, "RPC failed. Code=%d, Message=%s\n", status.error_code(),
          status.error_message().c_str());
  std::move(on_done).Run();
}

}  // namespace remoting
