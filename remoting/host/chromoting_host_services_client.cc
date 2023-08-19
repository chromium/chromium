// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/sid.h"
#include "remoting/host/win/acl_util.h"
#endif

namespace remoting {

namespace {

bool g_initialized = false;

mojo::PendingRemote<mojom::ChromotingHostServices> ConnectToServer() {
  auto server_name = GetChromotingHostServicesServerName();
  auto endpoint = named_mojo_ipc_server::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    LOG(WARNING) << "Cannot connect to IPC through server name " << server_name
                 << ". Endpoint is invalid.";
    return {};
  }
#if BUILDFLAG(IS_WIN)
  DWORD peer_session_id;
  if (!GetNamedPipeServerSessionId(endpoint.platform_handle().GetHandle().get(),
                                   &peer_session_id)) {
    PLOG(ERROR) << "GetNamedPipeServerSessionId failed";
    return {};
  }
  // '0' (default) corresponds to the session the network process runs in.
  if (peer_session_id != 0) {
    LOG(ERROR)
        << "Cannot establish connection with IPC server running in session: "
        << peer_session_id;
    return {};
  }
#endif
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto message_pipe =
      invitation.ExtractMessagePipe(kChromotingHostServicesMessagePipeId);
  return mojo::PendingRemote<mojom::ChromotingHostServices>(
      std::move(message_pipe), /* version= */ 0);
}

}  // namespace

#if BUILDFLAG(IS_LINUX)

// static
constexpr char
    ChromotingHostServicesClient::kChromeRemoteDesktopSessionEnvVar[];

#endif

ChromotingHostServicesClient::ChromotingHostServicesClient()
    : ChromotingHostServicesClient(base::Environment::Create(),
                                   base::BindRepeating(&ConnectToServer)) {
  DCHECK(g_initialized)
      << "ChromotingHostServicesClient::Initialize() has not been called.";
}

ChromotingHostServicesClient::ChromotingHostServicesClient(
    std::unique_ptr<base::Environment> environment,
    ConnectToServerCallback connect_to_server)
    : environment_(std::move(environment)),
      connect_to_server_(std::move(connect_to_server)) {}

ChromotingHostServicesClient::~ChromotingHostServicesClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool ChromotingHostServicesClient::Initialize() {
  DCHECK(!g_initialized);
#if BUILDFLAG(IS_WIN)
  // The network process running under the LocalService account verifies the
  // session ID of the client process, which normally isn't allowed since the
  // network process has reduced trust level, so we add an ACL to allow it.
  g_initialized = AddProcessAccessRightForWellKnownSid(
      base::win::WellKnownSid::kLocalService,
      PROCESS_QUERY_LIMITED_INFORMATION);
#else
  // Other platforms don't need initialization.
  g_initialized = true;
#endif
  return g_initialized;
}

mojom::ChromotingSessionServices*
ChromotingHostServicesClient::GetSessionServices() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!const_cast<ChromotingHostServicesClient*>(this)
           ->EnsureSessionServicesBinding()) {
    return nullptr;
  }
  return session_services_remote_.get();
}

bool ChromotingHostServicesClient::EnsureConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return true;
  }

  auto pending_remote = connect_to_server_.Run();
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    return false;
  }
  remote_.Bind(std::move(pending_remote));
  remote_.set_disconnect_handler(base::BindOnce(
      &ChromotingHostServicesClient::OnDisconnected, base::Unretained(this)));
  return true;
}

bool ChromotingHostServicesClient::EnsureSessionServicesBinding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_services_remote_.is_bound()) {
    return true;
  }
#if BUILDFLAG(IS_LINUX)
  if (!environment_->HasVar(kChromeRemoteDesktopSessionEnvVar)) {
    LOG(WARNING) << "Current desktop environment is not remotable.";
    return false;
  }
#endif
  if (!EnsureConnection()) {
    return false;
  }
  remote_->BindSessionServices(
      session_services_remote_.BindNewPipeAndPassReceiver());
  session_services_remote_.set_disconnect_handler(
      base::BindOnce(&ChromotingHostServicesClient::OnSessionDisconnected,
                     base::Unretained(this)));
  return true;
}

void ChromotingHostServicesClient::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_.reset();
}

void ChromotingHostServicesClient::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  session_services_remote_.reset();

  if (on_session_disconnected_callback_for_testing_) {
    std::move(on_session_disconnected_callback_for_testing_).Run();
  }
}

}  // namespace remoting
