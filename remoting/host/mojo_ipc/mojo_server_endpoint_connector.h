// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_H_

#include <memory>

#include "base/process/process_handle.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace remoting {

// Interface to allow platform-specific implementations to establish connection
// between the server endpoint and the client. mojo::IsolatedConnection can
// take a PlatformChannelServerEndpoint directly, but our implementations allow:
// 1. Reliably knowing when a new invitation needs to be sent; with the
//    alternative approach, the best we could do is to wait for an incoming IPC
//    call, which isn't reliable since a (malicious) client may clog the channel
//    by connecting and hanging without making any IPCs.
// 2. Observing the client process' PID without passing it via IPC, which
//    wouldn't be feasible with the alternative approach, since mojo doesn't
//    expose the underlying socket/named pipe.
class MojoServerEndpointConnector {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the client has connected to the server endpoint.
    virtual void OnServerEndpointConnected(
        std::unique_ptr<mojo::IsolatedConnection> connection,
        mojo::ScopedMessagePipeHandle message_pipe,
        base::ProcessId peer_pid) = 0;

    // Called when error occurred during the connection process.
    virtual void OnServerEndpointConnectionFailed() = 0;

   protected:
    Delegate() = default;
  };

  // Creates the platform-specific MojoServerEndpointConnector. |delegate| must
  // outlives the created object.
  static std::unique_ptr<MojoServerEndpointConnector> Create(
      Delegate* delegate);

  virtual ~MojoServerEndpointConnector() = default;

  // Connects to |server_endpoint|; invokes the delegate when it's connected or
  // failed to connect. Note that only one pending server endpoint is allowed.
  virtual void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) = 0;

 protected:
  MojoServerEndpointConnector() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_H_
