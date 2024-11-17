// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_SERVER_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_SERVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "components/named_mojo_ipc_server/named_mojo_message_pipe_server.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

namespace remoting {

// Class to run a mojo server on a named pipe for the ChromotingHostServices
// interface. Client processes must connect using ChromotingHostServicesClient.
class ChromotingHostServicesServer {
 public:
  using BindChromotingHostServicesCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::ChromotingHostServices>,
      base::ProcessId /* peer_pid */)>;

  explicit ChromotingHostServicesServer(
      BindChromotingHostServicesCallback bind_chromoting_host_services);
  ~ChromotingHostServicesServer();

  void StartServer();
  void StopServer();

 private:
  friend class ChromotingHostServicesServerTest;

  using Validator = base::RepeatingCallback<bool(
      const named_mojo_ipc_server::ConnectionInfo&)>;

  ChromotingHostServicesServer(
      const mojo::NamedPlatformChannel::ServerName& server_name,
      Validator validator,
      BindChromotingHostServicesCallback bind_chromoting_host_services);

  void OnMessagePipeReady(
      mojo::ScopedMessagePipeHandle message_pipe,
      std::unique_ptr<named_mojo_ipc_server::ConnectionInfo> connection_info,
      void* context,
      std::unique_ptr<mojo::IsolatedConnection> connection);

  named_mojo_ipc_server::NamedMojoMessagePipeServer message_pipe_server_;
  BindChromotingHostServicesCallback bind_chromoting_host_services_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_SERVER_H_
