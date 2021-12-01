// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_IPC_SERVER_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_IPC_SERVER_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "remoting/host/mojo_ipc/ipc_server.h"
#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector.h"

namespace mojo {
class IsolatedConnection;
}

namespace remoting {

// Template-less base class to keep implementations in the .cc file. For usage,
// see MojoIpcServer.
class MojoIpcServerBase : public IpcServer,
                          public MojoServerEndpointConnector::Delegate {
 public:
  // Internal use only.
  struct PendingConnection;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;

  // Sets a callback to be run when an invitation is sent. Used by unit tests
  // only.
  void set_on_invitation_sent_callback_for_testing(
      const base::RepeatingClosure& callback) {
    on_invitation_sent_callback_for_testing_ = callback;
  }

  size_t GetNumberOfActiveConnectionsForTesting() const {
    return active_connections_.size();
  }

 protected:
  explicit MojoIpcServerBase(
      const mojo::NamedPlatformChannel::ServerName& server_name);
  ~MojoIpcServerBase() override;

  void SendInvitation();

  void OnIpcDisconnected();

  virtual mojo::ReceiverId TrackMessagePipe(
      mojo::ScopedMessagePipeHandle message_pipe,
      base::ProcessId peer_pid) = 0;

  virtual void UntrackMessagePipe(mojo::ReceiverId id) = 0;

  virtual void UntrackAllMessagePipes() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingClosure disconnect_handler_;

 private:
  void OnServerEndpointCreated(mojo::PlatformChannelServerEndpoint endpoint);

  // MojoServerEndpointConnector::Delegate implementation.
  void OnServerEndpointConnected(
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::ScopedMessagePipeHandle message_pipe,
      base::ProcessId peer_pid) override;
  void OnServerEndpointConnectionFailed() override;

  using ActiveConnectionMap =
      base::flat_map<mojo::ReceiverId,
                     std::unique_ptr<mojo::IsolatedConnection>>;

  mojo::NamedPlatformChannel::ServerName server_name_;

  bool server_started_ = false;

  // A task runner to run blocking jobs.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_;

  std::unique_ptr<MojoServerEndpointConnector> endpoint_connector_;
  ActiveConnectionMap active_connections_;
  base::OneShotTimer resent_invitation_on_error_timer_;

  base::RepeatingClosure on_invitation_sent_callback_for_testing_;

  base::WeakPtrFactory<MojoIpcServerBase> weak_factory_{this};
};

// A helper that uses a NamedPlatformChannel to send out mojo invitations and
// maintains multiple concurrent IPCs. It keeps one outgoing invitation at a
// time and will send a new invitation whenever the previous one has been
// accepted by the client.
//
// Example usage:
//
//   class MyInterfaceImpl: public mojom::MyInterface {
//     void Start() {
//       server_.set_disconnect_handler(
//           base::BindRepeating(&MyInterfaceImpl::OnDisconnected, this));
//       server_.StartServer();
//     }

//     void OnDisconnected() {
//       LOG(INFO) << "Receiver disconnected: " << server_.current_receiver();
//     }

//     // mojom::MyInterface Implementation.
//     void DoWork() override {
//       // Do something...

//       // If you want to close the connection:
//       server_.Close(server_.current_receiver());
//     }

//     MojoIpcServer<mojom::MyInterface> server_{"my_server_name", this};
//   };
template <typename Interface>
class MojoIpcServer final : public MojoIpcServerBase {
 public:
  // server_name: The server name to start the NamedPlatformChannel.
  // message_pipe_id: The message pipe ID. The client must call
  // ExtractMessagePipe() with the same ID.
  MojoIpcServer(const mojo::NamedPlatformChannel::ServerName& server_name,
                Interface* interface_impl)
      : MojoIpcServerBase(server_name), interface_impl_(interface_impl) {
    receiver_set_.set_disconnect_handler(base::BindRepeating(
        &MojoIpcServer::OnIpcDisconnected, base::Unretained(this)));
  }

  ~MojoIpcServer() override = default;

  MojoIpcServer(const MojoIpcServer&) = delete;
  MojoIpcServer& operator=(const MojoIpcServer&) = delete;

  void set_disconnect_handler(base::RepeatingClosure handler) override {
    disconnect_handler_ = handler;
  }

  mojo::ReceiverId current_receiver() const override {
    return receiver_set_.current_receiver();
  }

  base::ProcessId current_peer_pid() const override {
    return receiver_set_.current_context();
  }

 private:
  // MojoIpcServerBase implementation.
  mojo::ReceiverId TrackMessagePipe(mojo::ScopedMessagePipeHandle message_pipe,
                                    base::ProcessId peer_pid) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return receiver_set_.Add(
        interface_impl_,
        mojo::PendingReceiver<Interface>(std::move(message_pipe)), peer_pid);
  }

  void UntrackMessagePipe(mojo::ReceiverId id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_set_.Remove(id);
  }

  void UntrackAllMessagePipes() override { receiver_set_.Clear(); }

  raw_ptr<Interface> interface_impl_;
  mojo::ReceiverSet<Interface, base::ProcessId> receiver_set_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_IPC_SERVER_H_
