// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_H_
#define IPC_IPC_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom-forward.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_message_pipe_reader.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/types.h>
#endif

namespace IPC {

class Listener;
class MojoBootstrap;
class UrgentMessageObserver;

//------------------------------------------------------------------------------
// See
// http://www.chromium.org/developers/design-documents/inter-process-communication
// for overview of IPC in Chromium.

// Channels are implemented using mojo message pipes.

class COMPONENT_EXPORT(IPC) Channel final
    : public internal::MessagePipeReader::Delegate {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Flags to test modes
  using ModeFlags = int;
  static constexpr ModeFlags MODE_NO_FLAG = 0x0;
  static constexpr ModeFlags MODE_SERVER_FLAG = 0x1;
  static constexpr ModeFlags MODE_CLIENT_FLAG = 0x2;

  // Some Standard Modes
  // TODO(morrita): These are under deprecation work. You should use Create*()
  // functions instead.
  enum Mode {
    MODE_NONE = MODE_NO_FLAG,
    MODE_SERVER = MODE_SERVER_FLAG,
    MODE_CLIENT = MODE_CLIENT_FLAG,
  };

  // Initialize a Channel.
  //
  // |channel_handle| identifies the communication Channel. For POSIX, if
  // the file descriptor in the channel handle is != -1, the channel takes
  // ownership of the file descriptor and will close it appropriately, otherwise
  // it will create a new descriptor internally.
  // |listener| receives a callback on the current thread for each newly
  // received message.
  //
  // There are four type of modes how channels operate:
  //
  // - Server and named server: In these modes, the Channel is
  //   responsible for setting up the IPC object.
  // - An "open" named server: It accepts connections from ANY client.
  //   The caller must then implement their own access-control based on the
  //   client process' user Id.
  // - Client and named client: In these mode, the Channel merely
  //   connects to the already established IPC object.
  //
  static std::unique_ptr<Channel> Create(
      mojo::ScopedMessagePipeHandle handle,
      Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  ~Channel();

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  //
  // The subclass implementation must call WillConnect() at the beginning of its
  // implementation.
  [[nodiscard]] bool Connect();

  // Pause the channel. Subsequent sends will be queued internally until
  // Unpause() is called and the channel is flushed either by Unpause() or a
  // subsequent call to Flush().
  void Pause();

  // Unpause the channel. This allows subsequent Send() calls to transmit
  // messages immediately, without queueing. If |flush| is true, any
  // messages queued while paused will be flushed immediately upon
  // unpausing. Otherwise you must call Flush() explicitly.
  //
  // Not all implementations support Unpause(). See ConnectPaused() above
  // for details.
  void Unpause(bool flush);

  // Manually flush the pipe. This is only useful exactly once, and only
  // after a call to Unpause(false), in order to explicitly flush out any
  // messages which were queued prior to unpausing.
  //
  // Not all implementations support Flush(). See ConnectPaused() above for
  // details.
  void Flush();

  // Close this Channel explicitly.  May be called multiple times.
  // On POSIX calling close on an IPC channel that listens for connections
  // will cause it to close any accepted connections, and it will stop
  // listening for new connections. If you just want to close the currently
  // accepted connection and listen for new ones, use
  // ResetToAcceptingConnectionState.
  void Close();

  // Channel support for associated Mojo interfaces.
  using GenericAssociatedInterfaceFactory =
      base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

  // Returns a ThreadSafeForwarded for this channel which can be used to
  // safely send mojom::Channel requests from arbitrary threads.
  std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
  CreateThreadSafeChannel();

  // Adds an interface factory to this channel for interface |name|. Must be
  // safe to call from any thread.
  void AddGenericAssociatedInterface(
      const std::string& name,
      const GenericAssociatedInterfaceFactory& factory);

  // Requests an associated interface from the remote endpoint.
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver);

  // Sets the UrgentMessageObserver for this channel. `observer` must
  // outlive the channel.
  //
  // Only channel associated mojo interfaces support urgent messages.
  void SetUrgentMessageObserver(UrgentMessageObserver* observer);

  // MessagePipeReader::Delegate
  void OnPeerPidReceived(int32_t peer_pid) override;
  void OnPipeError() override;
  void OnAssociatedInterfaceRequest(
      mojo::GenericPendingAssociatedReceiver receiver) override;

  // Generates a channel ID that's non-predictable and unique.
  static std::string GenerateUniqueRandomChannelID();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Sandboxed processes live in a PID namespace, so when sending the IPC hello
  // message from client to server we need to send the PID from the global
  // PID namespace.
  static void SetGlobalPid(int pid);
  static int GetGlobalPid();
#endif

 private:
  Channel(mojo::ScopedMessagePipeHandle handle,
          Mode mode,
          Listener* listener,
          const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
          const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  void ForwardMessage(mojo::Message message);
  void FinishConnectOnIOThread();

  void WillConnect();

  bool did_start_connect_ = false;
  base::WeakPtr<Channel> weak_ptr_;

  // A TaskRunner which runs tasks on the Channel's owning thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const mojo::MessagePipeHandle pipe_;

  std::unique_ptr<MojoBootstrap> bootstrap_;
  raw_ptr<Listener, DanglingUntriaged> listener_;

  std::unique_ptr<internal::MessagePipeReader> message_reader_;

  base::Lock associated_interface_lock_;
  std::map<std::string, GenericAssociatedInterfaceFactory>
      associated_interfaces_;

  base::WeakPtrFactory<Channel> weak_factory_{this};
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_H_
