// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_ISOLATED_CONNECTION_H_
#define MOJO_PUBLIC_CPP_SYSTEM_ISOLATED_CONNECTION_H_

#include "base/process/process.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// IsolatedConnection establishes a one-off Mojo IPC connection between two
// processes. Unlike more common connections established by invitation
// (see OutgoingInvitation and IncomingInvitation), isolated connections
// do not result in the two processes becoming part of the same connected
// graph of processes. As such, any message pipe established over this
// connection can only be used for direct IPC between the two processes in
// question.
//
// This means that if one of the processes sends a Mojo handle (e.g. another
// message pipe endpoint) to the other process, the receiving process cannot
// pass that handle to yet another process in its own graph. This limitation is
// subtle and can be difficult to work around, so use of IsolatedConnection
// should be rare.
//
// This is primarily useful when you already have two established Mojo process
// graphs isolated form each other, and you want to do some IPC between two
// processes, one in each graph.
//
// A connection established via |Connect()|, and any opened message pipes
// spanning that connection, will remain valid and connected as long as this
// object remains alive.
class MOJO_CPP_SYSTEM_EXPORT IsolatedConnection {
 public:
  IsolatedConnection();

  IsolatedConnection(const IsolatedConnection&) = delete;
  IsolatedConnection& operator=(const IsolatedConnection&) = delete;

  ~IsolatedConnection();

  // Connects to a process at the other end of the channel. Returns a primordial
  // message pipe that can be used for Mojo IPC. The connection
  // will be connected to a corresponding peer pipe in the remote process.
  ScopedMessagePipeHandle Connect(PlatformChannelEndpoint endpoint);

  // Connects to a process at the other end of the channel. Returns a primordial
  // message pipe that can be used for Mojo IPC. The connection
  // will be connected to a corresponding peer pipe in the remote process.
  // `process` identifies the remote process.
  ScopedMessagePipeHandle Connect(PlatformChannelEndpoint endpoint,
                                  base::Process process);

  // Same as above but works with a server endpoint. The corresponding client
  // could use the above signature with NamedPlatformChannel::ConnectToServer.
  ScopedMessagePipeHandle Connect(PlatformChannelServerEndpoint endpoint);

 private:
  const base::UnguessableToken token_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_ISOLATED_CONNECTION_H_
