// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_SERVER_H_
#define REMOTING_HOST_MOJO_IPC_SERVER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace remoting {

// Template-less base class to keep implementations in the .cc file. For usage,
// see MojoIpcServer.
class MojoIpcServerBase {
 public:
  // Starts sending out mojo invitations and accepting IPCs. No-op if the server
  // is already started.
  void StartServer();

  // Stops sending out mojo invitations and accepting IPCs. No-op if the server
  // is already stopped.
  void StopServer();

 protected:
  explicit MojoIpcServerBase(
      const mojo::NamedPlatformChannel::ServerName& server_name,
      uint64_t message_pipe_id);
  virtual ~MojoIpcServerBase();

  void SendInvitation();

  virtual void CloseAllConnections() = 0;

  virtual void OnInvitationAccepted(
      mojo::ScopedMessagePipeHandle message_pipe) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void OnInvitationSent(mojo::ScopedMessagePipeHandle message_pipe);
  void OnMessagePipeReady(MojoResult result,
                          const mojo::HandleSignalsState& state);

  mojo::NamedPlatformChannel::ServerName server_name_;
  uint64_t message_pipe_id_;

  bool server_started_ = false;

  // A task runner to run blocking jobs.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_;

  // The message pipe will be "pending" until a client accepts the invitation.
  mojo::SimpleWatcher pending_message_pipe_watcher_;
  mojo::ScopedMessagePipeHandle pending_message_pipe_;

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

//     MojoIpcServer<mojom::MyInterface> server_{"my_server_name", 0, this};
//   };
template <typename Interface>
class MojoIpcServer final : public MojoIpcServerBase {
 public:
  // server_name: The server name to start the NamedPlatformChannel.
  // message_pipe_id: The message pipe ID. The client must call
  // ExtractMessagePipe() with the same ID.
  MojoIpcServer(const mojo::NamedPlatformChannel::ServerName& server_name,
                uint64_t message_pipe_id,
                Interface* interface_impl)
      : MojoIpcServerBase(server_name, message_pipe_id),
        interface_impl_(interface_impl) {}

  ~MojoIpcServer() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // StopServer() calls CloseAllConnections(), which is not implemented in
    // the base class, so this method can't be called in ~MojoIpcServerBase().
    StopServer();
  }

  MojoIpcServer(const MojoIpcServer&) = delete;
  MojoIpcServer& operator=(const MojoIpcServer&) = delete;

  // Close the receiver identified by |id| and disconnects the remote. No-op if
  // |id| doesn't exist or the receiver is already closed.
  //
  // Returns a boolean that indicates whether a receiver has be closed.
  bool Close(mojo::ReceiverId id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return receiver_set_.Remove(id);
  }

  // Sets a callback to be invoked any time a receiver is disconnected. You may
  // find out which receiver is being disconnected by calling
  // |current_receiver()|.
  void set_disconnect_handler(base::RepeatingClosure handler) {
    receiver_set_.set_disconnect_handler(handler);
  }

  // Call this method to learn which receiver has received the incoming IPC or
  // which receiver is being disconnected.
  mojo::ReceiverId current_receiver() const {
    return receiver_set_.current_receiver();
  }

 private:
  // MojoIpcServerBase implementation.
  void CloseAllConnections() override { receiver_set_.Clear(); }

  void OnInvitationAccepted(
      mojo::ScopedMessagePipeHandle message_pipe) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_set_.Add(interface_impl_, mojo::PendingReceiver<Interface>(
                                           std::move(message_pipe)));

    SendInvitation();
  }

  Interface* interface_impl_;
  mojo::ReceiverSet<Interface> receiver_set_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_SERVER_H_
